#!/usr/bin/env python3
import pathlib
import re
import sys
import hashlib

root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
required = [
    "CMakeLists.txt", "CMakePresets.json", "README.md", "RESEARCH.md", "Source/PluginProcessor.cpp",
    "Source/PluginEditor.cpp", "Source/dsp/BitRashDSP.cpp",
    "Tests/DSPTests.cpp", "Tests/PluginTests.cpp", "Tests/EditorTests.cpp",
    ".github/workflows/ci.yml", ".github/workflows/release.yml",
]
missing = [p for p in required if not (root / p).exists()]
if missing:
    raise SystemExit("missing files: " + ", ".join(missing))
text = "\n".join((root / p).read_text() for p in required)
checks = {
    "juce commit": "91ad83ae34a81e0833b1a2b0866f54846370ae53" in text,
    "juce-ci commit": "926c5bb10335c29503f92869169759ec18d71449" in text,
    "EHL design module": "add_subdirectory(modules/juce-ehl-design-module)" in text
        and "EHL::JuceDesign" in text,
    "stage target": "ehl_stage_products" in text,
    "no generic editor": "GenericAudioProcessorEditor" in (root / "Tests/EditorTests.cpp").read_text()
        and "new juce::GenericAudioProcessorEditor" not in text,
    "stable artifacts": "artifacts/plugin-release/macos-arm64" in (root / "README.md").read_text()
        and "artifacts/plugin-release/windows-x64" in (root / "README.md").read_text(),
    "no asset font dirs": not (root / "Assets").exists() and not (root / "Fonts").exists(),
    "product dsp": "Source/dsp/BitRashDSP.cpp" in (root / "CMakeLists.txt").read_text()
        and "FoundationDSP" not in text,
}
failed = [name for name, ok in checks.items() if not ok]
if failed:
    raise SystemExit("failed checks: " + ", ".join(failed))
if re.search(r"TemplatePlugin|MyPlugin", text):
    raise SystemExit("stale generic names found")

token_re = re.compile(r"[a-z0-9]+")
alnum_re = re.compile(r"[a-z0-9]")
separator_re = re.compile(
    r"(?:\\{1,2}(?:[sbdwnrt]|x(?:09|0a|0d|20|a0)|u(?:0009|000a|000d|0020|00a0)"
    r"|U(?:00000009|0000000a|0000000d|00000020|000000a0))(?:[+*?]|\{\d+(?:,\d*)?\})?)"
    r"|(?:%(?:09|0a|0d|20|a0)(?:[+*?]|\{\d+(?:,\d*)?\})?)"
    r"|(?:&(?:nbsp|\#(?:0*9|0*10|0*13|0*32|0*160|x0*(?:9|a|d|20|a0)));(?:[+*?]|\{\d+(?:,\d*)?\})?)"
    r"|(?:\[(?:\\{1,2}(?:[sbdwnrt]|x(?:09|0a|0d|20|a0)|u(?:0009|000a|000d|0020|00a0)"
    r"|U(?:00000009|0000000a|0000000d|00000020|000000a0))|[^\]])+\](?:[+*?]|\{\d+(?:,\d*)?\})?)"
    r"|(?:\(\?[:=!<][^)]*\))"
    r"|(?:[\\|+*?^$()[\]{}.,;:_/\-]+)",
    re.IGNORECASE | re.VERBOSE,
)
forbidden_token_windows = {
    "977c2908e358e8dcae6fbb4db30ba9c8270086a256010014f553a960855cf56b",
}
forbidden_compact_windows = {
    "df72b45f82869a738a4b6548b7860129cd368209ac73577210765c4b929b17ee",
}
forbidden_compact_windows_by_size = {
    3: {
        "338fd9894b114dba6235ea4f939c51c7bb7038dd4f79f4c9985c26ae5217e64d",
    },
    17: forbidden_compact_windows,
}


def sha256(value):
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def has_forbidden_text(value):
    tokens = token_re.findall(value.casefold())
    for index in range(0, max(0, len(tokens) - 3 + 1)):
        if sha256(" ".join(tokens[index:index + 3])) in forbidden_token_windows:
            return True
    for variant in (value, separator_re.sub(" ", value)):
        compact = "".join(alnum_re.findall(variant.casefold()))
        for size, digests in forbidden_compact_windows_by_size.items():
            for index in range(0, max(0, len(compact) - size + 1)):
                if sha256(compact[index:index + size]) in digests:
                    return True
    return False


for path in required:
    if has_forbidden_text(path) or has_forbidden_text((root / path).read_text()):
        raise SystemExit("public text guard failed")
print("scaffold validation passed")
