#include "plex/termination_trace.h"
#include <cassert>
int main() {
    int count=plex::termination_trace();
    assert(count>0 && count<=24);
    puts("Normal-context termination unwinding passed on host; signal-context/PS5 unwinding is not claimed");
}
