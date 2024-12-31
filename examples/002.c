int foo() {
    return 42;
}

int bar() {
    return foo();
}

int main() {
    return bar();
}
