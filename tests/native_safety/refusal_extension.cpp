// A real, local dynamic-library fixture; no engine dependency or network installer.
namespace lbug::main { class ClientContext; }
extern "C" const char* name() { return "MAESTRO_REFUSAL_FIXTURE"; }
extern "C" void init(lbug::main::ClientContext*) {}
