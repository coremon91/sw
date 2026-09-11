"""Make AJA 18.0's WMI cleanup scope valid under current MSVC.

Keep this narrowly scoped compatibility patch reproducible; do not edit the SDK
manually. The underlying AJA source is MIT licensed.
"""
from pathlib import Path
import sys

path = Path(sys.argv[1]) / "ajabase/system/windows/infoimpl.cpp"
text = path.read_text(encoding="utf-8-sig")
marker = "\t// SW: declarations precede all cleanup jumps."
if marker not in text:
    declarations = [
        "IWbemServices *pSvc = NULL;", "IClientSecurity* pSecurity = NULL;",
        "DWORD authnSvc = 0;", "DWORD authzSvc = 0;", "LPOLESTR serverPrincName = NULL;",
        "DWORD authnLevel = 0;", "DWORD impLevel = 0;", "RPC_AUTH_IDENTITY_HANDLE authInfo = NULL;",
        "DWORD ifCapabilites = 0;", "IEnumWbemClassObject* pEnumerator = NULL;",
        "IWbemClassObject *pClsObj = NULL;", "ULONG uReturn = 0;",
    ]
    for declaration in declarations:
        old = "\t" + declaration + "\n"
        if text.count(old) != 1:
            raise RuntimeError("Unexpected AJA source; compatibility patch was not applied: " + declaration)
        text = text.replace(old, "")
    anchor = '\tstd::string retVal = "Microsoft Windows";\n'
    if text.count(anchor) != 1:
        raise RuntimeError("Unexpected AJA WMI function")
    text = text.replace(anchor, anchor + marker + "\n" + "".join("\t" + d + "\n" for d in declarations))
    path.write_text(text, encoding="utf-8", newline="\n")
