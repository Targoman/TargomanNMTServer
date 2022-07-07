#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <marian/marian.h>
#include <marian/common/config_parser.h>
#pragma GCC diagnostic pop

#include "server.h"

int main(void) {
    clsNmtServer server("0.0.0.0", 8080);
    return 0;
}