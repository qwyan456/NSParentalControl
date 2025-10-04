#pragma once
//#include <stratosphere/sf.hpp>

namespace hos = ams::hos;

#define PCTRL_I_SERVICE_INTERFACE_INFO(C, H)                                                                                                                                                                            \
    AMS_SF_METHOD_INFO(C, H, 0, void, Ping, (), ())

AMS_SF_DEFINE_INTERFACE(alefbet::pctrl::srv, IService, PCTRL_I_SERVICE_INTERFACE_INFO, 0x10003103)