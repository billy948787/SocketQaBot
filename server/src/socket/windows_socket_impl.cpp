#include "socket/windows_socket_impl.hpp"

namespace qabot::socket {
std::mutex WindowsSocketImpl::_socketMutex;
int WindowsSocketImpl::_activeSocketInstance = 0;
}