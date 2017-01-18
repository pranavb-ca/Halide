#include "Halide.h"
#include <stdio.h>

using namespace Halide;
using namespace Internal;

int main(int argc, char **argv) {
    if (0) {
        int size = 1024;
        Func f("f");
        Expr width = size;
        Expr height = size;
        Expr first_bin = 1;
        Expr bin_size = 20;
        Expr number_of_bins = 64;

        Var x("x"), y("y"), c("c");

        f(x, y, c) = x + y + c;
        f.compute_root();

        // Compute log chroma histogram
        RDom r_f(0, width, 0, height);
        Expr r = cast<float>(f(r_f.x, r_f.y, 0));
        Expr g = cast<float>(f(r_f.x, r_f.y, 1));
        Expr b = cast<float>(f(r_f.x, r_f.y, 2));

        Expr log_r = log(r + 0.5f);
        Expr log_g = log(g + 0.5f);
        Expr log_b = log(b + 0.5f);

        Expr u = log_g - log_r;
        Expr v = log_g - log_b;

        Expr u_bin = cast<int>(round((u - first_bin) / bin_size)) %
                   number_of_bins;
        Expr v_bin = cast<int>(round((v - first_bin) / bin_size)) %
                   number_of_bins;

        Expr u_coord = min(select(u_bin < 0, u_bin + number_of_bins, u_bin),
                                 number_of_bins - 1);
        Expr v_coord = min(select(v_bin < 0, v_bin + number_of_bins, v_bin),
                                 number_of_bins - 1);

        Func histogram("histogram");
        histogram(x, y) = 0;
        histogram(v_coord, u_coord) += 1;

        Var p("p");
        Func intm = histogram.update(0).rfactor(r_f.y, p);
        intm.compute_root();
        intm.update(0).parallel(p);

        //intm.compute_at(histogram, r_f.y);
        //histogram.update(0).parallel(r_f.y);

        histogram.realize(size, size);
    }

    if (1) { // DenouserHalide.cc GetMaximumByPlane
        Func input("input");
        int x_size = 1024, y_size = 1024, z_size = 3;

        int extent_x = x_size;
        int extent_y = y_size;

        Var x("x"), y("y"), z("z");

        input(x, y, z) = x + y + z;
        input.compute_root();

        Target target = get_jit_target_from_environment();
        const int vector_size = target.natural_vector_size<float>();

        // Find the maximum of the input over the tile area.
        RDom column_dom(0, extent_y);
        Func max_by_column("max_by_column");
        max_by_column(x, z) = maximum(input(x, column_dom, z));

        // Reduce by vectors worth of the result, which can be vectorized.
        RDom row_dom(0, extent_x / vector_size);
        Func max_by_vector("max_by_vector");
        max_by_vector(x, z) = maximum(max_by_column(x + row_dom * vector_size, z));

        // Only the final reduction by the vector size is slow (scalar code).
        RDom vec_dom(0, vector_size);
        Func result("result");
        result(z) = maximum(max_by_vector(vec_dom, z));

        // Compute each column as we need it.
        max_by_column.compute_at(max_by_vector, x).vectorize(x, vector_size);
        max_by_vector.compute_root().vectorize(x, vector_size);
        result.compute_root();

        result.realize(z_size);
    }

    if (1) { // common_halide.h ArgMin2

        Func f("f");
        Expr min_x = 0;
        Expr extent_x = 100;
        Expr min_y = 0;
        Expr extent_y = 100;
        Target target = get_jit_target_from_environment();

        Var x("x"), y("y");

        f(x, y) = x + y;
        f.compute_root();

        // First compute the argmin of each column.
        Func argmin_y("argmin_y");
        argmin_y(x, _) = {
            min_y,
            f(x, min_y, _)
        };
        RDom sy(min_y + 1, extent_y - 1);
        Expr f_xsy = f(x, sy, _);
        Expr update_miny = f_xsy < argmin_y(x, _)[1];
            argmin_y(x, _) = {
            select(update_miny, sy, argmin_y(x, _)[0]),
            select(update_miny, f_xsy, argmin_y(x, _)[1])
        };

        // Now compute the argmin across the argmins of the columns.
        Func argmin("argmin");
        argmin(_) = {
            min_x,
            argmin_y(min_x, _)[0],
            argmin_y(min_x, _)[1]
        };
        RDom sx(min_x + 1, extent_x - 1);
        Expr update_min = argmin_y(sx, _)[1] < argmin(_)[2];
        argmin(_) = {
            select(update_min, sx, argmin(_)[0]),
            select(update_min, argmin_y(sx, _)[0], argmin(_)[1]),
            select(update_min, argmin_y(sx, _)[1], argmin(_)[2])
        };

        // Schedule, vectorize the columns of miny.
        // Get the innermost var of argmin to compute argmin_y at, or outermost if the
        // result has zero dimensions.
        Var outer = Var::outermost();
        if (argmin.dimensions() > 0) {
            outer = argmin.args().front();
        }

        int vector_width = target.natural_vector_size(f.output_types()[0]);
        argmin_y.compute_at(argmin, outer)
            .vectorize(x, vector_width);
        argmin_y.update(0)
            .vectorize(x, vector_width);

        argmin.realize();
    }

    printf("Success\n");
    return 0;
}