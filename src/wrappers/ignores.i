// There are many classes and functions that must be
// %ignore'd for the wrappers to build.
// A few others may be %ignore'd by choice.

// prevent wrapping of certain nested classes,
// or particular methods, because they
// use types from the enclosing class
%include nested_ignores.i

// prevent wrapping things that are declared yet not defined
%include declared_but_not_defined.i

// various other %ignore's
%include misc_ignores.i
