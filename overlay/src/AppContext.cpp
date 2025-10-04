#include "AppContext.h"

static AppContext g_ctx;

AppContext& getAppContext() {
    return g_ctx;
}