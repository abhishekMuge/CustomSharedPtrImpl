#include <iostream>
#include <memory>

using namespace std;

//helper class
class HelperClass {
public:
	int value;
	HelperClass(int value): value(value) {

		std::cout << "Helper class got created\n";
	}
	HelperClass() {
		std::cout << "Helper class got created\n";
	}

	~HelperClass() {
		std::cout << "Helper class delted\n";
	}
};

class B; // forward declaration

class A {
public:
    shared_ptr<B> b;
    ~A() {
        cout << "A destroyed\n";
    }
};

class B {
public:
    shared_ptr<A> a;
    ~B() {
        cout << "B destroyed\n";
    }
};

class B2; // forward declaration

class A2 {
public:
    shared_ptr<B2> b;
    ~A2() {
        cout << "A2 destroyed\n";
    }
};

class B2 {
public:
    weak_ptr<A2> a; // FIX: weak_ptr instead of shared_ptr
    ~B2() {
        cout << "B2 destroyed\n";
    }
};

class SmartPointerHelperClass {
public:
	void uniquePtrExample() {
		unique_ptr<HelperClass> p1 = make_unique<HelperClass>();

		unique_ptr<HelperClass> p2 = std::move(p1);
		p2->value = 10;
		if(!p1) {
			cout << "P1 is null now" << endl;
		}
		cout << "P2 value: " << p2->value << endl;
	}

	void sharedPtrExample() {
		shared_ptr<HelperClass> p1 = make_shared<HelperClass>(20);
		cout << "p1 use_count: " << p1.use_count() << endl;

		{
			shared_ptr<HelperClass> p2 = p1;
			cout << "After p2 copy, use_count: " << p1.use_count() << endl;

			{
				shared_ptr<HelperClass> p3 = p2;
				cout << "After p3 copy, use_count: " << p1.use_count() << endl;
			}

			cout << "After p3 destroyed, use_count: " << p1.use_count() << endl;
		}

		cout << "After p2 destroyed, use_count: " << p1.use_count() << endl;
	}

	void cyclicReferenceProblem() {
	    cout << "\n--- Cyclic Reference Problem ---\n";

	    shared_ptr<A> a = make_shared<A>();
	    shared_ptr<B> b = make_shared<B>();

	    a->b = b;
	    b->a = a;

	    cout << "Exiting function... destructors NOT called due to cycle\n";
	}

	void weakPtrExample() {
	    cout << "\n--- Weak_ptr Solution ---\n";

	    shared_ptr<A2> a = make_shared<A2>();
	    shared_ptr<B2> b = make_shared<B2>();

	    a->b = b;
	    b->a = a;

	    cout << "Exiting function... destructors WILL be called\n";
	}

};
