#pragma once

namespace util {
    template<class T>
    class Singleton : private T {
    private:
        Singleton();
        ~Singleton();

    public:
        static T &instance();
    };

    template <class T>
    inline Singleton<T>::Singleton() {
        /* no-op */
    }

    template <class T>
    inline Singleton<T>::~Singleton() {
        /* no-op */
    }

    template <class T>
    T &Singleton<T>::instance() {
        static Singleton<T> s_oT;
        return (s_oT);
    }
}

