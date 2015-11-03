
#include "cocaine/detail/conductor/action.hpp"

namespace cocaine { namespace isolate { namespace conductor {

namespace action {

void
cancellation_t::cancel() {
    if(auto action = m_parent.lock()){
        action->cancel();
    }
}

}


}}} // namespace cocaine::isolate::conductor
