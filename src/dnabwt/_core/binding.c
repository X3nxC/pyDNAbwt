#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "binding.h"
#include "core_api.h"
#include "../../c/util/util.h"

typedef struct py_ctx {
    PyObject *progress_cb;
    PyObject *signal_cb;
} py_ctx_t;

static PyObject *g_core_error = NULL;
static PyObject *g_interrupted_error = NULL;
static const char *DNABWT_INDEX_CAPSULE_NAME = "dnabwt.search_index";

static int py_progress_cb(void *user_data, uint64_t processed_chars, uint64_t total_chars) {
    py_ctx_t *ctx = (py_ctx_t *)user_data;
    int rc = 0;

    if (ctx == NULL || ctx->progress_cb == NULL) {
        return 0;
    }

    PyGILState_STATE gil = PyGILState_Ensure();
    PyObject *result = PyObject_CallFunction(ctx->progress_cb,
                                             "KK",
                                             (unsigned long long)processed_chars,
                                             (unsigned long long)total_chars);
    if (result == NULL) {
        rc = 1;
    } else {
        rc = PyObject_IsTrue(result) > 0 ? 1 : 0;
        Py_DECREF(result);
    }
    PyGILState_Release(gil);
    return rc;
}

static int py_signal_cb(void *user_data) {
    py_ctx_t *ctx = (py_ctx_t *)user_data;
    int rc = 0;

    PyGILState_STATE gil = PyGILState_Ensure();
    if (ctx != NULL && ctx->signal_cb != NULL) {
        PyObject *result = PyObject_CallNoArgs(ctx->signal_cb);
        if (result == NULL) {
            rc = 1;
        } else {
            rc = PyObject_IsTrue(result) > 0 ? 1 : 0;
            Py_DECREF(result);
        }
    } else {
        if (PyErr_CheckSignals() != 0) {
            rc = 1;
        }
    }
    PyGILState_Release(gil);
    return rc;
}

static int setup_ctx(dnabwt_core_ctx_t *ctx, py_ctx_t *pyctx, PyObject *progress_cb, PyObject *signal_cb) {
    dnabwt_core_ctx_init(ctx);

    pyctx->progress_cb = NULL;
    pyctx->signal_cb = NULL;

    if (progress_cb != Py_None) {
        if (!PyCallable_Check(progress_cb)) {
            PyErr_SetString(PyExc_TypeError, "progress_cb must be callable or None");
            return 0;
        }
        Py_INCREF(progress_cb);
        pyctx->progress_cb = progress_cb;
        dnabwt_core_ctx_set_progress(ctx, py_progress_cb, pyctx);
    }

    if (signal_cb != Py_None) {
        if (!PyCallable_Check(signal_cb)) {
            Py_XDECREF(pyctx->progress_cb);
            PyErr_SetString(PyExc_TypeError, "signal_cb must be callable or None");
            return 0;
        }
        Py_INCREF(signal_cb);
        pyctx->signal_cb = signal_cb;
    }
    dnabwt_core_ctx_set_signal(ctx, py_signal_cb, pyctx);
    return 1;
}

static void teardown_ctx(py_ctx_t *pyctx) {
    Py_XDECREF(pyctx->progress_cb);
    Py_XDECREF(pyctx->signal_cb);
}

static void raise_status(dnabwt_status_t status) {
    if (status == DNABWT_STATUS_INVALID_ARGUMENT) {
        PyErr_SetString(PyExc_ValueError, dnabwt_status_message(status));
    } else if (status == DNABWT_STATUS_INTERRUPTED) {
        PyErr_SetString(g_interrupted_error, dnabwt_status_message(status));
    } else {
        PyErr_SetString(g_core_error, dnabwt_status_message(status));
    }
}

static void py_index_capsule_destructor(PyObject *capsule) {
    dnabwt_search_index_t *index = (dnabwt_search_index_t *)PyCapsule_GetPointer(capsule, DNABWT_INDEX_CAPSULE_NAME);
    if (index != NULL) {
        dnabwt_core_search_index_free(index);
    }
}

static dnabwt_search_index_t *py_index_from_capsule(PyObject *capsule) {
    if (!PyCapsule_CheckExact(capsule)) {
        PyErr_SetString(PyExc_TypeError, "index must be a dnabwt search index capsule");
        return NULL;
    }
    return (dnabwt_search_index_t *)PyCapsule_GetPointer(capsule, DNABWT_INDEX_CAPSULE_NAME);
}

static PyObject *py_transform_from_text(PyObject *self, PyObject *args, PyObject *kwargs) {
    const char *text;
    Py_ssize_t text_len;
    PyObject *progress_cb = Py_None;
    PyObject *signal_cb = Py_None;
    static char *kwlist[] = {"text", "progress_cb", "signal_cb", NULL};
    dnabwt_core_ctx_t ctx;
    py_ctx_t pyctx;
    uint8_t *out = NULL;
    size_t out_len = 0;
    dnabwt_status_t status;
    PyObject *result;

    (void)self;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s#|OO", kwlist, &text, &text_len, &progress_cb, &signal_cb)) {
        return NULL;
    }
    if (!setup_ctx(&ctx, &pyctx, progress_cb, signal_cb)) {
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
    status = dnabwt_core_transform_text(&ctx, (const uint8_t *)text, (size_t)text_len, &out, &out_len);
    Py_END_ALLOW_THREADS

    teardown_ctx(&pyctx);
    if (status != DNABWT_STATUS_OK) {
        raise_status(status);
        return NULL;
    }

    result = PyBytes_FromStringAndSize((const char *)out, (Py_ssize_t)out_len);
    dnabwt_free(out);
    return result;
}

static PyObject *py_transform_from_file(PyObject *self, PyObject *args, PyObject *kwargs) {
    const char *path;
    PyObject *progress_cb = Py_None;
    PyObject *signal_cb = Py_None;
    static char *kwlist[] = {"path", "progress_cb", "signal_cb", NULL};
    dnabwt_core_ctx_t ctx;
    py_ctx_t pyctx;
    uint8_t *out = NULL;
    size_t out_len = 0;
    dnabwt_status_t status;
    PyObject *result;

    (void)self;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|OO", kwlist, &path, &progress_cb, &signal_cb)) {
        return NULL;
    }
    if (!setup_ctx(&ctx, &pyctx, progress_cb, signal_cb)) {
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
    status = dnabwt_core_transform_file(&ctx, path, &out, &out_len);
    Py_END_ALLOW_THREADS

    teardown_ctx(&pyctx);
    if (status != DNABWT_STATUS_OK) {
        raise_status(status);
        return NULL;
    }

    result = PyBytes_FromStringAndSize((const char *)out, (Py_ssize_t)out_len);
    dnabwt_free(out);
    return result;
}

static PyObject *py_inverse_from_bytes(PyObject *self, PyObject *args, PyObject *kwargs) {
    const char *data;
    Py_ssize_t data_len;
    PyObject *progress_cb = Py_None;
    PyObject *signal_cb = Py_None;
    static char *kwlist[] = {"data", "progress_cb", "signal_cb", NULL};
    dnabwt_core_ctx_t ctx;
    py_ctx_t pyctx;
    uint8_t *out = NULL;
    size_t out_len = 0;
    dnabwt_status_t status;
    PyObject *result;

    (void)self;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "y#|OO", kwlist, &data, &data_len, &progress_cb, &signal_cb)) {
        return NULL;
    }
    if (!setup_ctx(&ctx, &pyctx, progress_cb, signal_cb)) {
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
    status = dnabwt_core_inverse_text(&ctx, (const uint8_t *)data, (size_t)data_len, &out, &out_len);
    Py_END_ALLOW_THREADS

    teardown_ctx(&pyctx);
    if (status != DNABWT_STATUS_OK) {
        raise_status(status);
        return NULL;
    }

    result = PyUnicode_DecodeASCII((const char *)out, (Py_ssize_t)out_len, "strict");
    dnabwt_free(out);
    return result;
}

static PyObject *py_inverse_from_file(PyObject *self, PyObject *args, PyObject *kwargs) {
    const char *path;
    PyObject *progress_cb = Py_None;
    PyObject *signal_cb = Py_None;
    static char *kwlist[] = {"path", "progress_cb", "signal_cb", NULL};
    dnabwt_core_ctx_t ctx;
    py_ctx_t pyctx;
    uint8_t *out = NULL;
    size_t out_len = 0;
    dnabwt_status_t status;
    PyObject *result;

    (void)self;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|OO", kwlist, &path, &progress_cb, &signal_cb)) {
        return NULL;
    }
    if (!setup_ctx(&ctx, &pyctx, progress_cb, signal_cb)) {
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
    status = dnabwt_core_inverse_file(&ctx, path, &out, &out_len);
    Py_END_ALLOW_THREADS

    teardown_ctx(&pyctx);
    if (status != DNABWT_STATUS_OK) {
        raise_status(status);
        return NULL;
    }

    result = PyUnicode_DecodeASCII((const char *)out, (Py_ssize_t)out_len, "strict");
    dnabwt_free(out);
    return result;
}

static PyObject *py_search_count(PyObject *self, PyObject *args, PyObject *kwargs) {
    const char *data;
    Py_ssize_t data_len;
    const char *pattern;
    Py_ssize_t pattern_len;
    PyObject *progress_cb = Py_None;
    PyObject *signal_cb = Py_None;
    static char *kwlist[] = {"data", "pattern", "progress_cb", "signal_cb", NULL};
    dnabwt_core_ctx_t ctx;
    py_ctx_t pyctx;
    dnabwt_status_t status;
    size_t match_count = 0;

    (void)self;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "y#s#|OO", kwlist, &data, &data_len, &pattern, &pattern_len,
                                     &progress_cb, &signal_cb)) {
        return NULL;
    }
    if (!setup_ctx(&ctx, &pyctx, progress_cb, signal_cb)) {
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
    status = dnabwt_core_search_count(&ctx,
                                      (const uint8_t *)data,
                                      (size_t)data_len,
                                      (const uint8_t *)pattern,
                                      (size_t)pattern_len,
                                      &match_count);
    Py_END_ALLOW_THREADS

    teardown_ctx(&pyctx);
    if (status != DNABWT_STATUS_OK) {
        raise_status(status);
        return NULL;
    }

    return PyLong_FromSize_t(match_count);
}

static PyObject *py_build_search_index_from_encoded_file(PyObject *self, PyObject *args, PyObject *kwargs) {
    const char *path;
    Py_ssize_t block_size = 512;
    Py_ssize_t cache_count = 2;
    PyObject *progress_cb = Py_None;
    PyObject *signal_cb = Py_None;
    static char *kwlist[] = {"path", "block_size", "cache_count", "progress_cb", "signal_cb", NULL};
    dnabwt_core_ctx_t ctx;
    py_ctx_t pyctx;
    dnabwt_search_index_t *index = NULL;
    dnabwt_status_t status;

    (void)self;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|nnOO", kwlist,
                                     &path,
                                     &block_size,
                                     &cache_count,
                                     &progress_cb,
                                     &signal_cb)) {
        return NULL;
    }
    if (block_size <= 0 || cache_count <= 0) {
        PyErr_SetString(PyExc_ValueError, "block_size and cache_count must be > 0");
        return NULL;
    }

    if (!setup_ctx(&ctx, &pyctx, progress_cb, signal_cb)) {
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
    status = dnabwt_core_search_index_build_from_encoded_file(&ctx,
                                                              path,
                                                              (size_t)block_size,
                                                              (size_t)cache_count,
                                                              &index);
    Py_END_ALLOW_THREADS

    teardown_ctx(&pyctx);
    if (status != DNABWT_STATUS_OK) {
        raise_status(status);
        return NULL;
    }

    return PyCapsule_New(index, DNABWT_INDEX_CAPSULE_NAME, py_index_capsule_destructor);
}

static PyObject *py_search_with_index(PyObject *self, PyObject *args, PyObject *kwargs) {
    PyObject *capsule;
    const char *pattern;
    Py_ssize_t pattern_len;
    PyObject *progress_cb = Py_None;
    PyObject *signal_cb = Py_None;
    static char *kwlist[] = {"index", "pattern", "progress_cb", "signal_cb", NULL};
    dnabwt_search_index_t *index;
    dnabwt_core_ctx_t ctx;
    py_ctx_t pyctx;
    dnabwt_status_t status;
    size_t match_count = 0;

    (void)self;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "Os#|OO", kwlist,
                                     &capsule,
                                     &pattern,
                                     &pattern_len,
                                     &progress_cb,
                                     &signal_cb)) {
        return NULL;
    }

    index = py_index_from_capsule(capsule);
    if (index == NULL) {
        return NULL;
    }

    if (!setup_ctx(&ctx, &pyctx, progress_cb, signal_cb)) {
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
    status = dnabwt_core_search_index_count(&ctx,
                                            index,
                                            (const uint8_t *)pattern,
                                            (size_t)pattern_len,
                                            &match_count);
    Py_END_ALLOW_THREADS

    teardown_ctx(&pyctx);
    if (status != DNABWT_STATUS_OK) {
        raise_status(status);
        return NULL;
    }

    return PyLong_FromSize_t(match_count);
}

static PyObject *py_status_message(PyObject *self, PyObject *args) {
    int code;
    (void)self;
    if (!PyArg_ParseTuple(args, "i", &code)) {
        return NULL;
    }
    return PyUnicode_FromString(dnabwt_status_message((dnabwt_status_t)code));
}

static PyMethodDef module_methods[] = {
    {"transform_from_text", (PyCFunction)py_transform_from_text, METH_VARARGS | METH_KEYWORDS, "Transform DNA text into packed BWT+RLE bytes."},
    {"transform_from_file", (PyCFunction)py_transform_from_file, METH_VARARGS | METH_KEYWORDS, "Transform DNA file into packed BWT+RLE bytes."},
    {"inverse_from_bytes", (PyCFunction)py_inverse_from_bytes, METH_VARARGS | METH_KEYWORDS, "Inverse transform packed bytes into DNA text."},
    {"inverse_from_file", (PyCFunction)py_inverse_from_file, METH_VARARGS | METH_KEYWORDS, "Inverse transform packed file into DNA text."},
    {"search_count", (PyCFunction)py_search_count, METH_VARARGS | METH_KEYWORDS, "Count pattern matches from packed bytes."},
    {"build_search_index_from_encoded_file", (PyCFunction)py_build_search_index_from_encoded_file, METH_VARARGS | METH_KEYWORDS, "Build persistent FM index from encoded file path."},
    {"search_with_index", (PyCFunction)py_search_with_index, METH_VARARGS | METH_KEYWORDS, "Count matches with a persistent FM index capsule."},
    {"status_message", py_status_message, METH_VARARGS, "Get status message by code."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef module_def = {
    PyModuleDef_HEAD_INIT,
    "_core_native",
    "dnabwt core extension",
    -1,
    module_methods,
    NULL,
    NULL,
    NULL,
    NULL
};

PyMODINIT_FUNC PyInit__core_native(void) {
    PyObject *m = PyModule_Create(&module_def);
    if (m == NULL) {
        return NULL;
    }

    g_core_error = PyErr_NewException("dnabwt._core_native.CoreError", PyExc_RuntimeError, NULL);
    g_interrupted_error = PyErr_NewException("dnabwt._core_native.InterruptedError", PyExc_KeyboardInterrupt, NULL);
    if (g_core_error == NULL || g_interrupted_error == NULL) {
        Py_XDECREF(g_core_error);
        Py_XDECREF(g_interrupted_error);
        Py_DECREF(m);
        return NULL;
    }

    Py_INCREF(g_core_error);
    if (PyModule_AddObject(m, "CoreError", g_core_error) != 0) {
        Py_DECREF(g_core_error);
        Py_DECREF(g_interrupted_error);
        Py_DECREF(m);
        return NULL;
    }

    Py_INCREF(g_interrupted_error);
    if (PyModule_AddObject(m, "InterruptedError", g_interrupted_error) != 0) {
        Py_DECREF(g_interrupted_error);
        Py_DECREF(m);
        return NULL;
    }

    return m;
}
