
#include <typeinfo>
#include <iostream>
#include <cocaine/common.hpp>
#include <cocaine/idl/locator.hpp>
#include <cocaine/idl/logging.hpp>

#include <cocaine/rpc/result_of.hpp>
#include <cocaine/rpc/protocol.hpp>
#include <cocaine/rpc/dispatch.hpp>

#include <cocaine/traits/endpoint.hpp>
#include <cocaine/traits/graph.hpp>
#include <cocaine/traits/map.hpp>
#include <cocaine/traits/vector.hpp>

#include "cocaine/idl/conductor.hpp"


namespace cocaine {

//typedef io::event_traits<io::locator::resolve>::upstream_type resolve_result_tag;
typedef io::event_traits<io::log::verbosity>::upstream_type resolve_result_tag;

//typedef io::protocol<resolve_result_tag>::scope::value resolve_result_proto;
typedef io::protocol<resolve_result_tag>::scope::value resolve_result_proto;

typedef result_of<io::locator::connect>::type connect_result_type;
typedef result_of<io::locator::cluster>::type cluster_result_type;
typedef result_of<io::locator::routing>::type routing_result_type;
typedef result_of<io::log::verbosity>::type streaming_result_type;
typedef io::event_traits<io::locator::connect>::upstream_type connect_trait_upstream_type;
typedef io::locator::connect::upstream_type connect_upstream_type;

typedef io::event_traits<io::conductor::terminate>::upstream_type terminate_result_tag;

//typedef 

}


template<typename T>
struct A {
  typedef typename cocaine::io::protocol<T>::scope protocol;
  typedef typename protocol::value value;
};

int main(){
    //std::cout << typeid(cocaine::resolve_result_tag).name() << std::endl; 
  std::cout << typeid(cocaine::resolve_result_proto).name() << std::endl; 
  std::cout << typeid(A<cocaine::resolve_result_tag>::value).name() << std::endl; 
  std::cout << typeid(A<cocaine::terminate_result_tag>::value).name() << std::endl; 
  //std::cout << typeid(cocaine::connect_result_type).name() << std::endl; 
  //std::cout << typeid(cocaine::cluster_result_type).name() << std::endl; 
  //std::cout << typeid(cocaine::routing_result_type).name() << std::endl; 
  //std::cout << "upstream: " << typeid(cocaine::connect_trait_upstream_type).name() << std::endl; 
  //std::cout << "trait upstream: " << typeid(cocaine::connect_upstream_type).name() << std::endl; 
  //std::cout << "trait upstream: " << typeid(cocaine::connect_trait_upstream_type).name() << std::endl; 
  return 0;
}



