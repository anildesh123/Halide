#include "HalideRuntime.h"

#define INLINE inline __attribute__((always_inline))

// This cannot be a function because the result goes out of scope.
#define alloca __builtin_alloca

#ifndef MX_API_VER
#define MX_API_VER 0x07040000
#endif

struct mxArray;

// It is important to have the mex function pointer definitions in a
// namespace to avoid silently conflicting symbols with matlab at
// runtime.
namespace Halide {
namespace Runtime {
namespace mex {

enum { TMW_NAME_LENGTH_MAX = 64 };
enum { mxMAXNAM = TMW_NAME_LENGTH_MAX };

typedef bool mxLogical;
typedef int16_t mxChar;

enum mxClassID {
    mxUNKNOWN_CLASS = 0,
    mxCELL_CLASS,
    mxSTRUCT_CLASS,
    mxLOGICAL_CLASS,
    mxCHAR_CLASS,
    mxVOID_CLASS,
    mxDOUBLE_CLASS,
    mxSINGLE_CLASS,
    mxINT8_CLASS,
    mxUINT8_CLASS,
    mxINT16_CLASS,
    mxUINT16_CLASS,
    mxINT32_CLASS,
    mxUINT32_CLASS,
    mxINT64_CLASS,
    mxUINT64_CLASS,
    mxFUNCTION_CLASS,
    mxOPAQUE_CLASS,
    mxOBJECT_CLASS,
#if defined(_LP64) || defined(_WIN64)
    mxINDEX_CLASS = mxUINT64_CLASS,
#else
    mxINDEX_CLASS = mxUINT32_CLASS,
#endif

    mxSPARSE_CLASS = mxVOID_CLASS
};

enum mxComplexity {
    mxREAL,
    mxCOMPLEX
};

#ifdef BITS_32
typedef int mwSize;
typedef int mwIndex;
typedef int mwSignedIndex;
#else
typedef size_t mwSize;
typedef size_t mwIndex;
typedef ptrdiff_t mwSignedIndex;
#endif

typedef void (*mex_exit_fn)(void);
typedef void (*mxFunctionPtr)(int, mxArray **, int, const mxArray **);

WEAK mxClassID get_class_id(int32_t type_code, int32_t type_bits) {
    switch (type_code) {
    case halide_type_int:
        switch (type_bits) {
        case 1: return mxLOGICAL_CLASS;
        case 8: return mxINT8_CLASS;
        case 16: return mxINT16_CLASS;
        case 32: return mxINT32_CLASS;
        case 64: return mxINT64_CLASS;
        }
        return mxUNKNOWN_CLASS;
    case halide_type_uint:
        switch (type_bits) {
        case 1: return mxLOGICAL_CLASS;
        case 8: return mxUINT8_CLASS;
        case 16: return mxUINT16_CLASS;
        case 32: return mxUINT32_CLASS;
        case 64: return mxUINT64_CLASS;
        }
        return mxUNKNOWN_CLASS;
    case halide_type_float:
        switch (type_bits) {
        case 32: return mxSINGLE_CLASS;
        case 64: return mxDOUBLE_CLASS;
        }
        return mxUNKNOWN_CLASS;
    }
    return mxUNKNOWN_CLASS;
}

WEAK const char *get_class_name(mxClassID id) {
    switch (id) {
    case mxCELL_CLASS: return "cell";
    case mxSTRUCT_CLASS: return "struct";
    case mxLOGICAL_CLASS: return "logical";
    case mxCHAR_CLASS: return "char";
    case mxVOID_CLASS: return "void";
    case mxDOUBLE_CLASS: return "double";
    case mxSINGLE_CLASS: return "single";
    case mxINT8_CLASS: return "int8";
    case mxUINT8_CLASS: return "uint8";
    case mxINT16_CLASS: return "int16";
    case mxUINT16_CLASS: return "uint16";
    case mxINT32_CLASS: return "int32";
    case mxUINT32_CLASS: return "uint32";
    case mxINT64_CLASS: return "int64";
    case mxUINT64_CLASS: return "uint64";
    case mxFUNCTION_CLASS: return "function";
    case mxOPAQUE_CLASS: return "opaque";
    case mxOBJECT_CLASS: return "object";
    default: return "unknown";
    }
}

// Declare function pointers for the mex APIs.
#define MEX_FN(ret, func, args) ret (*func)args;
#ifndef BITS_32
# define MEX_FN_730(ret, func, func_730, args) MEX_FN(ret, func, args)
#else
# define MEX_FN_700(ret, func, func_700, args) MEX_FN(ret, func, args)
#endif
#include "mex_functions.h"

template <typename T>
INLINE T* get_data(mxArray *a) { return (T *)mxGetData(a); }
template <typename T>
INLINE const T* get_data(const mxArray *a) { return (const T *)mxGetData(a); }

template <typename T>
INLINE T get_symbol(void *user_context, const char *name) {
    T s = (T)find_symbol(name);
    if (s == NULL) {
        error(user_context) << "Matlab API not found: " << name << "\n";
        return NULL;
    }
    return s;
}

}  // namespace mex
}  // namespace Runtime
}  // namespace Halide

using namespace Halide::Runtime::mex;

extern "C" {

WEAK void halide_matlab_error(void *, const char *msg) {
    // Note that mexErrMsg/mexErrMsgIdAndTxt crash Matlab. It seems to
    // be a common problem, those APIs seem to be very fragile.
    mexPrintf("Error: %s", msg);
}

WEAK void halide_matlab_print(void *, const char *msg) {
    mexPrintf("%s", msg);
}

WEAK int halide_mex_init(void *user_context) {
    // Assume that if mexPrintf exists, we've already attempted initialization.
    if (mexPrintf != NULL) {
        return 0;
    }

    #define MEX_FN(ret, func, args) func = get_symbol<ret (*)args>(user_context, #func); if (!func) { return -1; }
    #ifndef BITS_32
    # define MEX_FN_730(ret, func, func_730, args) func = get_symbol<ret (*)args>(user_context, #func_730); if (!func) { return -1; }
    #else
    # define MEX_FN_700(ret, func, func_700, args) func = get_symbol<ret (*)args>(user_context, #func_700); if (!func) { return -1; }
    #endif
    #include "mex_functions.h"

    // Set up Halide's printing to go through Matlab. Also, don't exit on error.
    halide_set_custom_print(halide_matlab_print);
    halide_set_error_handler(halide_matlab_error);

    return 0;
}

// Do only as much validation as is necessary to avoid accidentally
// reinterpreting casting data.
WEAK int halide_mex_validate_argument(void *user_context, const halide_filter_argument_t *arg, const mxArray *arr) {
    if (mxIsComplex(arr)) {
        error(user_context) << "Complex argument not supported for parameter " << arg->name << ".\n";
        return -1;
    }
    int dim_count = mxGetNumberOfDimensions(arr);
    if (arg->kind == halide_argument_kind_input_scalar) {
        // Validate that the mxArray has all dimensions of extent 1.
        for (int i = 0; i < dim_count; i++) {
            if (mxGetDimensions(arr)[i] != 1) {
                error(user_context) << "Expected scalar argument for parameter " << arg->name << ".\n";
                return -1;
            }
        }
        if (arg->type_bits == 1) {
            // The pipeline expects a boolean, validate the argument is logical.
            if (!mxIsLogical(arr)) {
                error(user_context) << "Expected logical argument for scalar parameter " << arg->name
                                    << ", got " << mxGetClassName(arr) << ".\n";
                return -1;
            }
        } else {
            // If it isn't a boolean, validate the argument is numeric.
            if (!mxIsNumeric(arr)) {
                error(user_context) << "Expected numeric argument for scalar parameter " << arg->name
                                    << ", got " << mxGetClassName(arr) << ".\n";
                return -1;
            }
        }
    } else if (arg->kind == halide_argument_kind_input_buffer ||
               arg->kind == halide_argument_kind_output_buffer) {
        // Validate that the data type of a buffer matches exactly.
        mxClassID arg_class_id = get_class_id(arg->type_code, arg->type_bits);
        if (mxGetClassID(arr) != arg_class_id) {
            error(user_context) << "Expected type of class " << get_class_name(arg_class_id)
                                << " for argument " << arg->name
                                << ", got class " << mxGetClassName(arr) << ".\n";
            return -1;
        }
        // Validate that the dimensionality matches. Matlab is wierd
        // because matrices always have at least 2 dimensions, and it
        // truncates trailing dimensions of extent 1. So, the only way
        // to have an error here is to have more dimensions with
        // extent != 1 than the Halide pipeline expects.
        while (dim_count > 0 && mxGetDimensions(arr)[dim_count - 1] == 1) {
            dim_count--;
        }
        if (dim_count > arg->dimensions) {
            error(user_context) << "Expected array of rank " << arg->dimensions
                                << "for argument " << arg->name
                                << ", got array of rank " << dim_count << ".\n";
            return -1;
        }
    }
    return 0;
}

WEAK int halide_mex_validate_arguments(void *user_context, const halide_filter_metadata_t *metadata,
                                       int nlhs, mxArray **plhs, int nrhs, const mxArray **prhs) {
    // Validate the number of arguments is correct.
    if (nrhs != metadata->num_arguments) {
        error(user_context) << "Expected " << metadata->num_arguments
                            << " arguments for Halide pipeline " << metadata->name
                            << ", got " << nrhs << ".\n";
        return -1;
    }

    // Validate the LHS has zero or one argument.
    if (nlhs > 1) {
        error(user_context) << "Expected zero or one return value for Halide pipeline " << metadata->name
                            << ", got " << nlhs << ".\n";
        return -1;
    }

    // Validate each argument.
    for (int i = 0; i < nrhs; i++) {
        const mxArray *arg = prhs[i];
        int ret = halide_mex_validate_argument(user_context, &metadata->arguments[i], arg);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

// Convert a matlab mxArray to a Halide buffer_t, with a specific number of dimensions.
WEAK int halide_mex_array_to_buffer_t(const mxArray *arr, int expected_dims, buffer_t *buf) {
    memset(buf, 0, sizeof(buffer_t));
    buf->host = (uint8_t *)mxGetData(arr);
    buf->elem_size = mxGetElementSize(arr);

    // Matlab swaps the first two dimensions, so we need to keep at
    // least two dimensions for now.
    if (expected_dims < 2) expected_dims = 2;

    int dim_count = mxGetNumberOfDimensions(arr);
    for (int i = 0; i < dim_count && i < expected_dims; i++) {
        buf->extent[i] = static_cast<int32_t>(mxGetDimensions(arr)[i]);
    }

    // Add back the dimensions with extent 1.
    for (int i = 2; i < expected_dims; i++) {
        if (buf->extent[i] == 0) {
            buf->extent[i] = 1;
        }
    }

    swap(buf->extent[0], buf->extent[1]);

    // Compute dense strides.
    buf->stride[0] = 1;
    for (int i = 1; i < expected_dims; i++) {
        buf->stride[i] = buf->extent[i - 1] * buf->stride[i - 1];
    }

    return 0;
}

// Convert a matlab mxArray to a scalar.
WEAK int halide_mex_array_to_scalar(const mxArray *arr, int32_t type_code, int32_t type_bits, void *scalar) {
    switch (type_code) {
    case halide_type_int:
        switch (type_bits) {
        case 1: *reinterpret_cast<bool *>(scalar) = *get_data<uint8_t>(arr) != 0; return 0;
        case 8: *reinterpret_cast<int8_t *>(scalar) = *get_data<int8_t>(arr); return 0;
        case 16: *reinterpret_cast<int16_t *>(scalar) = *get_data<int16_t>(arr); return 0;
        case 32: *reinterpret_cast<int32_t *>(scalar) = *get_data<int32_t>(arr); return 0;
        case 64: *reinterpret_cast<int64_t *>(scalar) = *get_data<int64_t>(arr); return 0;
        }
        return -1;
    case halide_type_uint:
        switch (type_bits) {
        case 1: *reinterpret_cast<bool *>(scalar) = *get_data<uint8_t>(arr) != 0; return 0;
        case 8: *reinterpret_cast<uint8_t *>(scalar) = *get_data<uint8_t>(arr); return 0;
        case 16: *reinterpret_cast<uint16_t *>(scalar) = *get_data<uint16_t>(arr); return 0;
        case 32: *reinterpret_cast<uint32_t *>(scalar) = *get_data<uint32_t>(arr); return 0;
        case 64: *reinterpret_cast<uint64_t *>(scalar) = *get_data<uint64_t>(arr); return 0;
        }
        return -1;
    case halide_type_float:
        switch (type_bits) {
        case 32: *reinterpret_cast<float *>(scalar) = *get_data<float>(arr); return 0;
        case 64: *reinterpret_cast<double *>(scalar) = *get_data<double>(arr); return 0;
        }
        return -1;
    }
    return -1;
}

WEAK int halide_mex_call_pipeline(void *user_context,
                                  int (*pipeline)(void **args), const halide_filter_metadata_t *metadata,
                                  int nlhs, mxArray **plhs, int nrhs, const mxArray **prhs) {

    int32_t result_storage;
    int32_t *result_ptr = &result_storage;
    if (nlhs > 0) {
        plhs[0] = mxCreateNumericMatrix(1, 1, mxINT32_CLASS, mxREAL);
        result_ptr = get_data<int32_t>(plhs[0]);
    }
    int32_t &result = *result_ptr;

    result = halide_mex_init(user_context);
    if (result != 0) {
        return result;
    }

    result = halide_mex_validate_arguments(user_context, metadata, nlhs, plhs, nrhs, prhs);
    if (result != 0) {
        return result;
    }

    void **args = (void **)alloca(nrhs * sizeof(void *));
    for (int i = 0; i < nrhs; i++) {
        const mxArray *arg = prhs[i];
        const halide_filter_argument_t *arg_metadata = &metadata->arguments[i];

        if (arg_metadata->kind == halide_argument_kind_input_buffer ||
            arg_metadata->kind == halide_argument_kind_output_buffer) {
            buffer_t *buf = (buffer_t *)alloca(sizeof(buffer_t));
            result = halide_mex_array_to_buffer_t(arg, arg_metadata->dimensions, buf);
            if (result != 0) {
                return result;
            }
            args[i] = buf;
        } else {
            result = halide_mex_array_to_scalar(arg, arg_metadata->type_code, arg_metadata->type_bits, args[i]);
            if (result != 0) {
                return result;
            }
        }
    }

    result = pipeline(args);
    return result;
}

}  // extern "C"
