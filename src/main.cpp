
#include <iostream>

#include "unique_pointer_ref.hpp"
#include "CustomSharedPtr.hpp"

using namespace std;

//int main() {
//	auto smartPtrDemo = new SmartPointerHelperClass();
//	smartPtrDemo->uniquePtrExample();
//	smartPtrDemo->sharedPtrExample();
//	smartPtrDemo->cyclicReferenceProblem();
//	smartPtrDemo->weakPtrExample();
//	return 0;
//}

class Test {
public:
    Test() { cout << "Constructed\n"; }
    ~Test() { cout << "Destroyed\n"; }
};

int main() {
    SharedPtr<Test> p1(new Test());

    {
        SharedPtr<Test> p2 = p1;
        cout << "Count: " << p1.use_count() << endl;

        WeakPtr<Test> wp = p1;

        if (auto sp = wp.lock()) {
            cout << "Locked successfully\n";
        }
    }

    cout << "Count after scope: " << p1.use_count() << endl;

    return 0;
}
