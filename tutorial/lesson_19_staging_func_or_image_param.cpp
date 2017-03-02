// Halide tutorial lesson 19: Staging a Func or an ImageParam

// This lesson demonstrates how to use Func::in and ImageParam::in to
// schedule a Func differently in different places and to stage loads from
// a Func or an ImageParam.

// On linux, you can compile and run it like so:
// g++ lesson_19*.cpp -g -I ../include -L ../bin -lHalide -lpthread -ldl -o lesson_19 -std=c++11
// LD_LIBRARY_PATH=../bin ./lesson_19

// On os x:
// g++ lesson_19*.cpp -g -I ../include -L ../bin -lHalide -o lesson_19 -std=c++11
// DYLD_LIBRARY_PATH=../bin ./lesson_19

// If you have the entire Halide source tree, you can also build it by
// running:
//    make tutorial_lesson_19_staging_func_or_image_param
// in a shell with the current directory at the top of the halide
// source tree.

// The only Halide header file you need is Halide.h. It includes all of Halide.
#include "Halide.h"

// We'll also include stdio for printf.
#include <stdio.h>

using namespace Halide;

int main(int argc, char **argv) {
    // First we'll declare some Vars to use below.
    Var x("x"), y("y");

    // This lesson will be about "wrapping" a Func or an ImageParam using the
    // Func::in and ImageParam::in directives

    {
        // Consider the following pipeline:
        Func f("f_local"), g("g_local"), h("h_local");
        f(x, y) = x + y;
        g(x, y) = 2 * f(x, y);
        h(x, y) = 3 + f(x, y);
        f.compute_root();
        g.compute_root();
        h.compute_root();
        // which is equivalent to the following loop nests:
        // for y:
        //   for x:
        //     f(x, y) = x + y
        // for y:
        //   for x:
        //     g(x, y) = 2 * f(x, y)
        // for y:
        //   for x:
        //     h(x, y) = 3 + f(x, y)

        // Now, let's do the following:
        Func f_in_g = f.in(g);
        f_in_g.compute_root();
        // Equivalently, we could also chain the schedules like so:
        // f.in(g).compute_root();

        g.realize(5, 5);
        h.realize(5, 5);
        // See figures/lesson_19_wrapper_local_g.gif and
        // figures/lesson_19_wrapper_local_h.gif for a visualization of
        // what this did.
    }

    {
        // f.in(g) replaces all calls to 'f' inside 'g' with a unique wrapper
        // Func and then returns that wrapper. Essentially, it rewrites the
        // original pipeline above into the following:
        Func f_in_g("f_in_g"), f("f"), g("g"), h("h");
        f(x, y) = x + y;
        f_in_g(x, y) = f(x, y);
        g(x, y) = 2 * f_in_g(x, y);
        h(x, y) = 3 + f(x, y);
        f.compute_root();
        g.compute_root();
        h.compute_root();
        // which is equivalent to the following loop nests:
        // for y:
        //   for x:
        //     f(x, y) = x + y
        // for y:
        //   for x:
        //     f_in_g(x, y) = f(x, y)
        // for y:
        //   for x:
        //     g(x, y) = 2 * f_in_g(x, y)
        // for y:
        //   for x:
        //     h(x, y) = 3 + f(x, y)

        // Note that only calls to 'f' inside 'g' are replaced. The ones inside
        // 'h' remain unchanged.
    }

    {
        Func f("f_global"), g("g_global"), h("h_global");
        f(x, y) = x + y;
        g(x, y) = 2 * f(x, y);
        h(x, y) = 3 + f(x, y);
        f.compute_root();
        g.compute_root();
        h.compute_root();
        // If we want to replace all calls to 'f' inside all functions in the
        // pipeline with calls to the wrapper, we could do it like so:
        f.in().compute_root();
        // This will create and return a global wrapper. All calls to 'f' inside
        // 'g' and 'h' will be replaced with calls to this global wrapper.

        // The equivalent loop nests are the following:
        // for y:
        //   for x:
        //     f(x, y) = x + y
        // for y:
        //   for x:
        //     f_global_wrapper(x, y) = f(x, y)
        // for y:
        //   for x:
        //     g(x, y) = 2 * f_global_wrapper(x, y)
        // for y:
        //   for x:
        //     h(x, y) = 3 + f_global_wrapper(x, y)

        g.realize(5, 5);
        h.realize(5, 5);
        // See figures/lesson_19_wrapper_global_g.gif and
        // figures/lesson_19_wrapper_global_h.gif for a visualization of
        // what this did.
    }


    // Func::in and ImageParam::in can be used for variety of scheduling tricks.
    {
        // Say we have the following pipeline:
        Func g("g_sched"), f1("f1_sched"), f2("f2_sched");
        g(x, y) = x + y;
        f1(x, y) = 2 * g(x, y);
        f2(x, y) = 3 + g(x, y);
        // and we want to schedule 'g' differently depending on whether it is
        // used by 'f1' or 'f2': in 'f1', we want to vectorize 'g' across the
        // x dimension, while in 'f2', we want to parallelize it. It is possible
        // to do this in Halide using the in directive.

        // g.in(f1) replaces all calls to 'g' inside 'f1' with a unique wrapper
        // Func and then returns that wrapper. Then, we schedule the wrapper to
        // be computed at 'f1' and vectorize it across the x dimension:
        g.in(f1).compute_at(f1, y).vectorize(x, 4);

        // Similarly, to parallelize 'g' across the x dimension inside 'f2', we
        // do the following:
        g.in(f2).compute_at(f2, y).parallel(x);

        Buffer<int> halide_result_f1 = f1.realize(8, 8);
        Buffer<int> halide_result_f2 = f2.realize(8, 8);

        // See figures/lesson_19_wrapper_vary_schedule_f1.gif and
        // figures/lesson_19_wrapper_vary_schedule_f2.gif for a visualization
        // of what this did.

        // The equivalent C is:
        int c_result_f1[8][8];
        for (int f1_y = 0; f1_y < 8; f1_y++) {
            int g_in_f1[8];
            for (int g_x_o = 0; g_x_o < 2; g_x_o++) {
                /* vectorized */ for (int g_x_i = 0; g_x_i < 4; g_x_i++) {
                    g_in_f1[4 * g_x_o + g_x_i] = 4 * g_x_o + g_x_i + f1_y;
                }
            }
            for (int f1_x = 0; f1_x < 8; f1_x++) {
                c_result_f1[f1_y][f1_x] = 2 * g_in_f1[f1_x];
            }
        }

        int c_result_f2[8][8];
        for (int f2_y = 0; f2_y < 8; f2_y++) {
            int g_in_f2[8];
            /* parallel */ for (int g_x = 0; g_x < 8; g_x++) {
                g_in_f2[g_x] = g_x + f2_y;
            }
            for (int f2_x = 0; f2_x < 8; f2_x++) {
                c_result_f2[f2_y][f2_x] = 3 + g_in_f2[f2_x];
            }
        }

        // Check the results match:
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                if (halide_result_f1(x, y) != c_result_f1[y][x]) {
                    printf("halide_result_f1(%d, %d) = %d instead of %d\n",
                           x, y, halide_result_f1(x, y), c_result_f1[y][x]);
                    return -1;
                }
            }
        }
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                if (halide_result_f2(x, y) != c_result_f2[y][x]) {
                    printf("halide_result_f2(%d, %d) = %d instead of %d\n",
                           x, y, halide_result_f2(x, y), c_result_f2[y][x]);
                    return -1;
                }
            }
        }
    }

    {
        // Func::in is useful to stage loads from a Func via some intermediate
        // buffer (perhaps on the stack or in shared GPU memory).

        // Let's say 'f' is really expensive to compute and used in several
        // places. We don't want to recompute 'f' often, but since it is large,
        // it does not fit in the cache. To deal with this issue, we can do
        // the following:
        Func f("f_load"), g("g_load");
        f(x, y) = x + y;
        g(x, y) = 2 * f(y, x);
        // First, compute 'f' at root.
        f.compute_root();
        // Then, we use Func::in to stage the loads from 'f' in tiles
        // as necessary:
        Var xo("xo"), xi("xi"), yo("yo"), yi("yi");
        g.tile(x, y, xo, yo, xi, yi, 4, 4);
        f.in(g).compute_at(g, xo);

        Buffer<int> halide_result = g.realize(8, 8);

        // See figures/lesson_19_wrapper_stage_func_loads.gif for a visualization of
        // what this did.

        // The equivalent C is:
        int f_buffer[8][8];
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                f_buffer[y][x] = x + y;
            }
        }

        int c_result[8][8];
        for (int yo = 0; yo < 2; yo++) {
            for (int xo = 0; xo < 2; xo++) {
                int f_in_g[4][4];
                for (int yi = 0; yi < 4; yi++) {
                    for (int xi = 0; xi < 4; xi++) {
                        f_in_g[yi][xi] = f_buffer[4 * yo + yi][4 * xo + xi];
                    }
                }
                for (int yi = 0; yi < 4; yi++) {
                    for (int xi = 0; xi < 4; xi++) {
                        c_result[4 * yo + yi][4 * xo + xi] = 2 * f_in_g[yi][xi];
                    }
                }
            }
        }

        // Check the results match:
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                if (halide_result(x, y) != c_result[y][x]) {
                    printf("halide_result(%d, %d) = %d instead of %d\n",
                           x, y, halide_result(x, y), c_result[y][x]);
                    return -1;
                }
            }
        }
    }

    {
        // Func::in can also be used to group multiple stages of a Func
        // in the same loop nest. Consider the following code:
        Func f("f_group"), g("g_group");
        f(x, y) = x + y;
        f(x, y) += x - y;
        g(x, y) = 2 * f(x, y);

        // When we schedule 'f' to be computed at root (by calling f.compute_root()),
        // all its stages are computed at separate loop nest:
        // for y:
        //   for x:
        //     f(x, y) = x + y
        // for y:
        //   for x:
        //     f(x, y) += x - y
        // for y:
        //   for x:
        //     g(x, y) = 2 * f(x, y)

        // We can use Func::in to group those stages to be computed at the
        // same loop nest like so:
        f.in(g).compute_root();
        // f.in(g) replaces all calls to 'f' inside 'g' with a unique wrapper
        // Func and then returns that wrapper, which is then scheduled to
        // be computed at root. By default, Func is computed inline, or if it
        // has updates, all of its stages will be computed at the innermost
        // loop of its consumer. In this case, all stages of 'f' will be
        // computed within the innermost loop of its wrapper, generating the
        // following loop nests:
        // for y:
        //   for x:
        //     f(x, y) = x + y
        //     f(x, y) += x - y
        //     f_in_g(x, y) = f(x, y)

        Buffer<int> halide_result = g.realize(8, 8);

        // See figures/lesson_19_wrapper_group_func_stages.gif for a visualization of
        // what this did.

        // The equivalent C is:
        int f_in_g[8][8];
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                int f = x + y;
                f += x - y;
                f_in_g[y][x] = f;
            }
        }
        int c_result_g[8][8];
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                c_result_g[y][x] = 2 * f_in_g[y][x];
            }
        }

        // Check the results match:
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                if (halide_result(x, y) != c_result_g[y][x]) {
                    printf("halide_result(%d, %d) = %d instead of %d\n",
                           x, y, halide_result(x, y), c_result_g[y][x]);
                    return -1;
                }
            }
        }
    }

    {
        // ImageParam::in behaves the same way as Func::in. We can also
        // use ImageParam::in to stage loads from an ImageParam via some
        // intermediate buffer (e.g. on the stack or in shared GPU memory).

        // The following example illustrates how you would use ImageParam::in
        // to stage loads from an ImageParam in tiles.
        ImageParam img(Int(32), 2, "img");
        Func f("f_img");
        f(x, y) = 2 * img(y, x);

        // First, we tile 'f' into 4x4 blocks.
        Var xo("xo"), xi("xi"), yo("yo"), yi("yi");
        f.compute_root().tile(x, y, xo, yo, xi, yi, 4, 4);
        // Then, we create a wrapper for 'img' which will load the corresponding
        // values from 'img' at tile level.
        Func img_wrapper = img.in().compute_at(f, xo);
        // If, for some reason, we want to unroll the first dimension of the
        // wrapper by 2, we can do the following:
        img_wrapper.unroll(_0, 2);
        // Note that since, unlike Func::in, anonymous wrapper Func created by
        // ImageParam::in does not have any explicitly named variables, we use
        // implicit variables to name the dimensions of the image wrapper:
        // _0 as the 1st dimension, _1 as the 2nd dimension, and so on.

        Buffer<int> input(8, 8);
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                input(x, y) = x + y;
            }
        }
        img.set(input);
        Buffer<int> halide_result = f.realize(8, 8);

        // See figures/lesson_19_wrapper_image_param.gif for a visualization of
        // what this did.

        // The equivalent C is:
        int c_result[8][8];
        for (int f_yo = 0; f_yo < 2; f_yo++) {
            for (int f_xo = 0; f_xo < 2; f_xo++) {
                int imgw[4][4];
                for (int imgw_yi = 0; imgw_yi < 4; imgw_yi++) {
                    for (int imgw_xi = 0; imgw_xi < 2; imgw_xi++) {
                        int x = 4 * f_xo + 2 * imgw_xi;
                        int y = 4 * f_yo + imgw_yi;
                        imgw[imgw_yi][2 * imgw_xi] = input(x, y);
                        imgw[imgw_yi][2 * imgw_xi + 1] = input(x + 1, y);
                    }
                }
                for (int f_yoi = 0; f_yoi < 4; f_yoi++) {
                    for (int f_xoi = 0; f_xoi < 4; f_xoi++) {
                        int x = 4 * f_xo + f_xoi;
                        int y = 4 * f_yo + f_yoi;
                        c_result[y][x] = 2 * imgw[f_yoi][f_xoi];
                    }
                }
            }
        }

        // Check the results match:
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                if (halide_result(x, y) != c_result[y][x]) {
                    printf("halide_result(%d, %d) = %d instead of %d\n",
                           x, y, halide_result(x, y), c_result[y][x]);
                    return -1;
                }
            }
        }
    }

    printf("Success!\n");

    return 0;
}