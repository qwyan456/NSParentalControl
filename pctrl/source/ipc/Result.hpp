#ifndef IPC_RESULT_HPP
#define IPC_RESULT_HPP

// These are all possible 'result' values returned by any aspect of the IPC system
namespace Ipc {
    // Possible results
    enum class Result {
        Ok,       
        Error,          
        BadInput,           
        SubQueueFull,       
        UnknownCommand,
        Unknown
    };
};

#endif