#include "HalideRuntime.h"
#include "printer.h"

extern "C" {

WEAK int halide_error_bounds_inference_call_failed(void *user_context, const char *extern_stage_name, int result) {
    error(user_context)
        << "Bounds inference call to external stage " << extern_stage_name
        << " returned non-zero value: " << result;
    return result;
}

WEAK int halide_error_extern_stage_failed(void *user_context, const char *extern_stage_name, int result) {
    error(user_context)
        << "Call to external stage " << extern_stage_name
        << " returned non-zero value: " << result;
    return result;
}

WEAK int halide_error_explicit_bounds_too_small(void *user_context, const char *func_name, const char *var_name,
                                                int min_bound, int max_bound, int min_required, int max_required) {
    error(user_context)
        << "Bounds given for " << var_name << " in " << func_name
        << " (from " << min_bound << " to " << max_bound
        << ") do not cover required region (from " << min_required
        << " to " << max_required << ")";
    return halide_error_code_explicit_bounds_too_small;
}

WEAK int halide_error_bad_type(void *user_context, const char *func_name,
                               uint8_t code_given, uint8_t correct_code,
                               uint8_t bits_given, uint8_t correct_bits,
                               uint16_t lanes_given, uint16_t correct_lanes) {
    halide_type_t correct_type(halide_type_code_t(correct_code), correct_bits, correct_lanes);
    halide_type_t type_given(halide_type_code_t(code_given), bits_given, lanes_given);
    error(user_context)
        << func_name << " has type " << correct_type
        << " but type of the buffer passed in is " << type_given;
    return halide_error_code_bad_type;
}

WEAK int halide_error_access_out_of_bounds(void *user_context, const char *func_name,
                                           int dimension, int min_touched, int max_touched,
                                           int min_valid, int max_valid) {
    if (min_touched < min_valid) {
        error(user_context)
            << func_name << " is accessed at " << min_touched
            << ", which is before the min (" << min_valid
            << ") in dimension " << dimension;
    } else if (max_touched > max_valid) {
        error(user_context)
            << func_name << " is accessed at " << max_touched
            << ", which is beyond the max (" << max_valid
            << ") in dimension " << dimension;
    }
    return halide_error_code_access_out_of_bounds;
}

WEAK int halide_error_buffer_allocation_too_large(void *user_context, const char *buffer_name, uint64_t allocation_size, uint64_t max_size) {
    error(user_context)
        << "Total allocation for buffer " << buffer_name
        << " is " << allocation_size
        << ", which exceeds the maximum size of " << max_size;
    return halide_error_code_buffer_allocation_too_large;
}

WEAK int halide_error_buffer_extents_negative(void *user_context, const char *buffer_name, int dimension, int extent) {
    error(user_context)
        << "The extents for buffer " << buffer_name
        << " dimension " << dimension
        << " is negative (" << extent << ")";
    return halide_error_code_buffer_extents_negative;
}

WEAK int halide_error_buffer_extents_too_large(void *user_context, const char *buffer_name, int64_t actual_size, int64_t max_size) {
    error(user_context)
        << "Product of extents for buffer " << buffer_name
        << " is " << actual_size
        << ", which exceeds the maximum size of " << max_size;
    return halide_error_code_buffer_extents_too_large;
}

WEAK int halide_error_constraints_make_required_region_smaller(void *user_context, const char *buffer_name,
                                                               int dimension,
                                                               int constrained_min, int constrained_extent,
                                                               int required_min, int required_extent) {
    int required_max = required_min + required_extent - 1;
    int constrained_max = constrained_min + required_extent - 1;
    error(user_context)
        << "Applying the constraints on " << buffer_name
        << " to the required region made it smaller. "
        << "Required size: " << required_min << " to " << required_max << ". "
        << "Constrained size: " << constrained_min << " to " << constrained_max << ".";
    return halide_error_code_constraints_make_required_region_smaller;
}

WEAK int halide_error_constraint_violated(void *user_context, const char *var, int val,
                                          const char *constrained_var, int constrained_val) {
    error(user_context)
        << "Constraint violated: " << var << " (" << val
        << ") == " << constrained_var << " (" << constrained_val << ")";
    return halide_error_code_constraint_violated;
}

WEAK int halide_error_param_too_small_i64(void *user_context, const char *param_name,
                                          int64_t val, int64_t min_val) {
    error(user_context)
        << "Parameter " << param_name
        << " is " << val
        << " but must be at least " << min_val;
    return halide_error_code_param_too_small;
}

WEAK int halide_error_param_too_small_u64(void *user_context, const char *param_name,
                                          uint64_t val, uint64_t min_val) {
    error(user_context)
        << "Parameter " << param_name
        << " is " << val
        << " but must be at least " << min_val;
    return halide_error_code_param_too_small;
}

WEAK int halide_error_param_too_small_f64(void *user_context, const char *param_name,
                                          double val, double min_val) {
    error(user_context)
        << "Parameter " << param_name
        << " is " << val
        << " but must be at least " << min_val;
    return halide_error_code_param_too_small;
}

WEAK int halide_error_param_too_large_i64(void *user_context, const char *param_name,
                                          int64_t val, int64_t max_val) {
    error(user_context)
        << "Parameter " << param_name
        << " is " << val
        << " but must be at most " << max_val;
    return halide_error_code_param_too_large;
}

WEAK int halide_error_param_too_large_u64(void *user_context, const char *param_name,
                                          uint64_t val, uint64_t max_val) {
    error(user_context)
        << "Parameter " << param_name
        << " is " << val
        << " but must be at most " << max_val;
    return halide_error_code_param_too_large;
}

WEAK int halide_error_param_too_large_f64(void *user_context, const char *param_name,
                                          double val, double max_val) {
    error(user_context)
        << "Parameter " << param_name
        << " is " << val
        << " but must be at most " << max_val;
    return halide_error_code_param_too_large;
}

WEAK int halide_error_out_of_memory(void *user_context) {
    // The error message builder uses malloc, so we can't use it here.
    halide_error(user_context, "Out of memory (halide_malloc returned NULL)");
    return halide_error_code_out_of_memory;
}

WEAK int halide_error_buffer_argument_is_null(void *user_context, const char *buffer_name) {
    error(user_context)
        << "Buffer argument " << buffer_name << " is NULL";
    return halide_error_code_buffer_argument_is_null;
}

WEAK int halide_error_debug_to_file_failed(void *user_context, const char *func,
                                           const char *filename, int error_code) {
    error(user_context)
        << "Failed to dump function " << func
        << " to file " << filename
        << " with error " << error_code;
    return halide_error_code_debug_to_file_failed;
}

WEAK int halide_error_failed_to_upgrade_buffer_t(void *user_context,
                                                 const char *name,
                                                 const char *reason) {
    error(user_context)
        << "Failed to upgrade buffer_t to halide_buffer_t for " << name << ": " << reason;
    return halide_error_code_failed_to_upgrade_buffer_t;
}

WEAK int halide_error_failed_to_downgrade_buffer_t(void *user_context,
                                                 const char *name,
                                                 const char *reason) {
    error(user_context)
        << "Failed to downgrade halide_buffer_t to buffer_t for " << name << ": " << reason;
    return halide_error_code_failed_to_downgrade_buffer_t;
}

WEAK int halide_error_unaligned_host_ptr(void *user_context, const char *func,
                                         int alignment) {
    error(user_context)
        << "The host pointer of " << func
        << " is not aligned to a " << alignment
        << " bytes boundary.";
    return halide_error_code_unaligned_host_ptr;
}

WEAK int halide_error_bad_fold(void *user_context, const char *func_name, const char *var_name,
                               const char *loop_name) {
    error(user_context)
        << "The folded storage dimension " << var_name << " of " << func_name
        << " was accessed out of order by loop " << loop_name << ".";
    return halide_error_code_bad_fold;
}

WEAK int halide_error_bad_extern_fold(void *user_context, const char *func_name,
                                      int dim, int min, int extent, int valid_min, int fold_factor) {
    if (min < valid_min || min + extent > valid_min + fold_factor) {
        error(user_context)
            << "Cannot fold dimension " << dim << " of " << func_name
            << " because an extern stage accesses [" << min << ", " << (min + extent - 1) << "],"
            << " which is outside the range currently valid: ["
            << valid_min << ", " << (valid_min + fold_factor - 1) << "].";
    } else {
        error(user_context)
            << "Cannot fold dimension " << dim << " of " << func_name
            << " because an extern stage accesses [" << min << ", " << (min + extent - 1) << "],"
            << " which wraps around the boundary of the fold, "
            << "which occurs at multiples of " << fold_factor << ".";
    }
    return halide_error_code_bad_extern_fold;
}

WEAK int halide_error_fold_factor_too_small(void *user_context, const char *func_name, const char *var_name,
                                            int fold_factor, const char *loop_name, int required_extent) {
    error(user_context)
        << "The fold factor (" << fold_factor
        << ") of dimension " << var_name << " of " << func_name
        << " is too small to store the required region accessed by loop "
        << loop_name << " (" << required_extent << ").";
    return halide_error_code_fold_factor_too_small;
}

WEAK int halide_error_requirement_failed(void *user_context, const char *condition, const char *message) {
    error(user_context)
        << "Requirement Failed: (" << condition << ") " << message;
    return halide_error_code_requirement_failed;
}

WEAK int halide_error_specialize_fail(void *user_context, const char *message) {
    error(user_context)
        << "A schedule specialized with specialize_fail() was chosen: " << message;
    return halide_error_code_specialize_fail;
}


}  // extern "C"
