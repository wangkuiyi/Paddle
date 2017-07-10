#include <boost/python.hpp>

char const* greet() { return "hello, world"; }

BOOST_PYTHON_MODULE(hello_ext) { boost::python::def("pypaddle", greet); }
