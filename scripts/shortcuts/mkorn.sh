cd $OPENDIHU_HOME && scons BUILD_TYPE=release no_tests=True && (cd - && echo opendihu release build without tests succeeded) || (echo opendihu release build without tests failed && cd - && exit -1)
