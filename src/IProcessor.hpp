#pragma once

#include "IRegisterable.hpp"

namespace pgp {

    class IProcessor : virtual public IRegisterable {
    public:
        virtual void step(float time, float delta) = 0;
    };
}
