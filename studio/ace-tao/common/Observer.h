// Observer.h

#ifndef Observer_h
#define Observer_h

#include <string>

class Observer
{
public:
    virtual void Observe(std::string msg) = 0;

    virtual ~Observer() {}
};

#endif //#ifndef Observer_h
