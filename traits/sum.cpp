#include <iostream>
using namespace std;

class IntArray {
public:
    IntArray() {
        a = new int[10];
        for (int i = 0; i < 10; ++i)
        {
            a[i] = i+1;
        }
    }
    ~IntArray() {
        delete[] a;
    }

    int GetSum(int times) {
        int sum = 0;
        for (int i = 0; i < 10; ++i) {
            sum += a[i];
        }
        return sum * times;
    }

private:
    int* a;
};

class FloatArray {
public:
    FloatArray() {
        a = new float[10];
        for (int i = 0; i < 10; ++i)
        {
            a[i] = (i * 1.0f) / 10;
        }
    }
    ~FloatArray() {
        delete[] a;
    }

    float GetSum(float times) {
        float sum = 0.0f;
        for (int i = 0; i < 10; ++i) {
            sum += a[i];
        }
        return sum * times;
    }

private:
    float* a;
};

template <class IterT>
struct my_iterator_traits {
    typedef typename IterT::value_type value_type;
};

template <class IterT>
struct my_iterator_traits<IterT *> {
    typedef IterT value_type;
};

template <class T>
class NumTraits {
};

template <>
class NumTraits<IntArray> {
    public:
    typedef int ret_type;
    typedef int arg_type;
};

template <>
class NumTraits<FloatArray> {
    public:
    typedef float ret_type;
    typedef float arg_type;
};

template <class T>
class Sum {
    public:
    typename NumTraits<T>::ret_type GetSum(T& obj, typename NumTraits<T>::arg_type arg) {
        return obj.GetSum(arg);
    }
};

int main() {
    IntArray iarr;
    FloatArray farr;

    Sum<IntArray> si;
    Sum<FloatArray> sf;

    cout << "the 3 times of the sum of int array: " << si.GetSum(iarr, 3) << endl;
    cout << "the 3.2 times of the sum of float array: " << sf.GetSum(farr, 3.2f) << endl;

    return 0;
}