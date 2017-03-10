#include <algorithm>
#include <map>
#include <string>

#include "Prefetch.h"
#include "IRMutator.h"
#include "Bounds.h"
#include "Scope.h"
#include "Util.h"

namespace Halide {
namespace Internal {

using std::map;
using std::pair;
using std::set;
using std::string;
using std::vector;

namespace {

const Definition &get_stage_definition(const Function &f, int stage_num) {
    if (stage_num == 0) {
        return f.definition();
    }
    return f.update(stage_num - 1);
}

class InjectPrefetch : public IRMutator {
public:
    InjectPrefetch(const map<string, Function> &e)
        : env(e), current_func(nullptr), stage(-1) { }

private:
    const map<string, Function> &env;
    const Function *current_func;
    int stage;
    Scope<Interval> scope;
    Scope<Box> buffer_bounds;

private:
    using IRMutator::visit;

    const vector<PrefetchDirective> &get_prefetch_list(const string &loop_name) {
        if (!current_func || !starts_with(loop_name, current_func->name() + ".s" + std::to_string(stage))) {
            vector<string> v = split_string(loop_name, ".");
            internal_assert(v.size() > 2);
            const string &func_name = v[0];

            // Get the stage index
            stage = -1;
            internal_assert(v[1].substr(0, 1) == "s");
            {
                string str = v[1].substr(1, v[1].size() - 1);
                bool has_only_digits = (str.find_first_not_of( "0123456789" ) == string::npos);
                internal_assert(has_only_digits);
                stage = atoi(str.c_str());
            }
            internal_assert(stage >= 0);

            const auto &it = env.find(func_name);
            internal_assert(it != env.end());

            current_func = &it->second;
            internal_assert(stage <= (int)current_func->updates().size());
        }

        const Definition &def = get_stage_definition(*current_func, stage);
        return def.schedule().prefetches();
    }

    const Box &get_buffer_bounds(string name, int dims) {
        if (buffer_bounds.contains(name)) {
            const Box &b = buffer_bounds.ref(name);
            internal_assert((int)b.size() == dims);
            return b;
        }

        // It is an image
        internal_assert(env.find(name) == env.end());
        Box b;
        for (int i = 0; i < dims; i++) {
            string dim_name = std::to_string(i);
            Expr buf_min_i = Variable::make(Int(32), name + ".min." + dim_name);
            Expr buf_extent_i = Variable::make(Int(32), name + ".extent." + dim_name);
            Expr buf_max_i = buf_min_i + buf_extent_i - 1;
            b.push_back(Interval(buf_min_i, buf_max_i));
        }
        buffer_bounds.push(name, b);
        return buffer_bounds.ref(name);
    }

    void visit(const Realize *op) {
        Box b;
        b.used = op->condition;
        for (const auto &r : op->bounds) {
            b.push_back(Interval(r.min, r.min + r.extent - 1));
        }
        buffer_bounds.push(op->name, b);
        IRMutator::visit(op);
        buffer_bounds.pop(op->name);
    }

    void visit(const Let *op) {
        Interval in = bounds_of_expr_in_scope(op->value, scope);
        scope.push(op->name, in);
        IRMutator::visit(op);
        scope.pop(op->name);
    }

    void visit(const LetStmt *op) {
        Interval in = bounds_of_expr_in_scope(op->value, scope);
        scope.push(op->name, in);
        IRMutator::visit(op);
        scope.pop(op->name);
    }

    Stmt add_prefetch(string buf_name, const Parameter &param, const Box &box, Stmt body) {
        // Construct the region to be prefetched.
        Region bounds;
        for (size_t i = 0; i < box.size(); i++) {
            Expr extent = box[i].max - box[i].min + 1;
            bounds.push_back(Range(box[i].min, extent));
        }

        Stmt prefetch;
        if (param.defined()) {
            prefetch = Prefetch::make(buf_name, {param.type()}, bounds, param);
        } else {
            const auto &it = env.find(buf_name);
            internal_assert(it != env.end());
            prefetch = Prefetch::make(buf_name, it->second.output_types(), bounds);
        }

        if (box.maybe_unused()) {
            prefetch = IfThenElse::make(box.used, prefetch);
        }
        return Block::make({prefetch, body});
    }

    void visit(const For *op) {
        const Function *old_func = current_func;
        int old_stage = stage;

        const vector<PrefetchDirective> &prefetch_list = get_prefetch_list(op->name);

        // Add loop variable to interval scope for any inner loop prefetch
        Expr loop_var = Variable::make(Int(32), op->name);
        scope.push(op->name, Interval(loop_var, loop_var));
        Stmt body = mutate(op->body);
        scope.pop(op->name);

        if (!prefetch_list.empty()) {
            // If there are multiple prefetches of the same Func or ImageParam,
            // use the most recent one
            set<string> seen;
            for (int i = prefetch_list.size() - 1; i >= 0; --i) {
                const PrefetchDirective &p = prefetch_list[i];
                if (!ends_with(op->name, "." + p.var) || (seen.find(p.name) != seen.end())) {
                    continue;
                }
                seen.insert(p.name);

                // Add loop variable + prefetch offset to interval scope for box computation
                Expr fetch_at = loop_var + p.offset;
                scope.push(op->name, Interval(fetch_at, fetch_at));
                map<string, Box> boxes_rw = boxes_touched(body, scope);
                scope.pop(op->name);

                // TODO(psuriana): Only prefetch the newly accessed data. We
                // should subtract the box accessed during previous iteration
                // from the one accessed during this iteration.

                // TODO(psuriana): Shift the base address of the prefetched box
                // instead of intersecting the extents with the bounds to simplify
                // the extent exprs. We may have a higher chance a collapsing the
                // box this way.
                const auto &b = boxes_rw.find(p.name);
                if (b != boxes_rw.end()) {
                    // Only prefetch the region that is in bounds.
                    const Box &bounds = get_buffer_bounds(b->first, b->second.size());
                    Box prefetch_box = box_intersection(b->second, bounds);

                    body = add_prefetch(b->first, p.param, prefetch_box, body);
                }
            }
        }

        if (!body.same_as(op->body)) {
            stmt = For::make(op->name, op->min, op->extent, op->for_type, op->device_api, body);
        } else {
            stmt = op;
        }

        current_func = old_func;
        stage = old_stage;
    }
};

} // namespace

Stmt inject_prefetch(Stmt s, const map<string, Function> &env) {
    return InjectPrefetch(env).mutate(s);
}

}
}
