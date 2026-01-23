#include <QCoreApplication>
#include <iostream>


void __attribute__((constructor)) premain() {
  std::cout << "[Test] GLOBAL CONSTRUCTOR RUNNING" << std::endl;
}

int main(int argc, char *argv[]) {
  std::cout << "[Test] main() entered" << std::endl;
  return 0;
}
