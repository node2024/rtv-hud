"""Regression for the startup hang. Uses the pinned Metamod search implementation.
Run: python3 tests/interface_lookup_test.py /path/to/deps/reference
No game process or network connection is needed.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
deps = Path(sys.argv[1])
source = (root / "src/plugin.cpp").read_text()
assert re.search(r"GET_V_IFACE_CURRENT\(GetEngineFactory, networkServer, INetworkServerService,\s*NETWORKSERVERSERVICE_INTERFACE_VERSION\)", source)
header = (deps / "hl2sdk-cs2/public/interfaces/interfaces.h").read_text()
version = re.search(r'#define\s+NETWORKSERVERSERVICE_INTERFACE_VERSION\s+"([^"]+)"', header).group(1)
assert re.fullmatch(r"NetworkServerService_\d{3}", version)
metamod = (deps / "metamod-source/core/metamod.cpp").read_text()
start = metamod.index("int MetamodSource::FormatIface(")
end = metamod.index("void *MetamodSource::VInterfaceMatch", start)
implementation = metamod[start:end].replace("MetamodSource::", "")
program = """#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
using CreateInterfaceFn = void* (*)(const char*, int*);
#define META_IFACE_FAILED 1
""" + implementation + """
void* factory(const char* name, int*) {
    static int service;
    return !strcmp(name, "%s") ? &service : nullptr;
}
int main(int argc, char** argv) {
    int result = 0;
    return argc != 2 || !InterfaceSearch(factory, argv[1], 999, &result);
}
""" % version
with tempfile.TemporaryDirectory() as temp:
    cpp = Path(temp) / "lookup.cpp"
    binary = Path(temp) / "lookup"
    cpp.write_text(program)
    subprocess.run(["c++", "-O2", str(cpp), "-o", str(binary)], check=True)
    try:
        subprocess.run([str(binary), "NetworkServerService"], timeout=1, check=True)
    except subprocess.TimeoutExpired:
        print("Original lookup: startup hang reproduced")
    else:
        raise AssertionError("Expected original unversioned lookup to hang")
    subprocess.run([str(binary), version], timeout=1, check=True)
    print("Fixed SDK interface lookup: resolved successfully")
