%module ffmpeg_lib
%include "stdint.i"
%{
#include "ts_muxer.h"
%}

%typemap(in) int [ANY] (int temp[$1_dim0]) {
    int i;
    if (!PySequence_Check($input)) {
        PyErr_SetString(PyExc_ValueError,"Expected a sequence");
        return NULL;
    }
    if (PySequence_Length($input) != $1_dim0) {
        PyErr_SetString(PyExc_ValueError,"Size mismatch. Expected $1_dim0 elements");
        return NULL;
    }
    for (i = 0; i < $1_dim0; i++) {
        PyObject *o = PySequence_GetItem($input,i);
        if (PyNumber_Check(o)) {
            temp[i] = (int) PyLong_AsLong(o);
        } else {
            PyErr_SetString(PyExc_ValueError,"Sequence elements must be numbers");
            return NULL;
        }
    }
    $1 = temp;
}

%typemap(in) (const unsigned char*) {
  if (!PyByteArray_Check($input)) {
    SWIG_exception_fail(SWIG_TypeError, "in method '" "$symname" "', argument "
                       "$argnum"" of type '" "$type""'");
  }
  $1 = (const unsigned char*) PyByteArray_AsString($input);
}

%include <pybuffer.i>
%pybuffer_string(unsigned char*)
%include "carrays.i"
%array_functions(int, intArray)
%array_functions(unsigned int, uintArray)
%array_functions(char*, char_p_Array)
%include "ts_muxer.h"


