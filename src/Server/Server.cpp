// Server.cpp : Defines the entry point for the application.
//

#include <iostream>
#include <pybind11/pybind11.h>

namespace py = pybind11;

int meaning_of_life() {
    return 42;
}

PYBIND11_MODULE(Server, m) {
    m.doc() = "Example pybind11 module";
    m.def("meaning_of_life", &meaning_of_life, "Returns the meaning of life.");
}
