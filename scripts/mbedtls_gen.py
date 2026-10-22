#!/usr/bin/env python3
"""Regenerate app/third-party/mbedtls-gen after updating the third-party/mbedtls submodule.

Needs CMake, Perl and a Python with jinja2 and jsonschema (pass it with --python).
"""

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MBEDTLS = ROOT / "third-party" / "mbedtls"
DEST = ROOT / "app" / "third-party" / "mbedtls-gen"
FILES = [
    "library/error.c",
    "library/mbedtls_config_check_before.h",
    "library/mbedtls_config_check_final.h",
    "library/mbedtls_config_check_user.h",
    "library/ssl_debug_helpers_generated.c",
    "library/version_features.c",
    "tf-psa-crypto/core/psa_crypto_driver_wrappers.h",
    "tf-psa-crypto/core/psa_crypto_driver_wrappers_no_static.c",
    "tf-psa-crypto/core/tf_psa_crypto_config_check_before.h",
    "tf-psa-crypto/core/tf_psa_crypto_config_check_final.h",
    "tf-psa-crypto/core/tf_psa_crypto_config_check_user.h",
]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--python", default=sys.executable)
    ap.add_argument("--perl", default=shutil.which("perl") or "perl")
    args = ap.parse_args()

    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run(["cmake", "-S", str(MBEDTLS), "-B", tmp, "-DGEN_FILES=ON",
                        "-DENABLE_PROGRAMS=OFF", "-DENABLE_TESTING=OFF",
                        f"-DPython3_EXECUTABLE={args.python}", f"-DPERL_EXECUTABLE={args.perl}"], check=True)
        subprocess.run(["cmake", "--build", tmp, "--target", "mbedtls"], check=True)
        for rel in FILES:
            out = DEST / rel
            out.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(Path(tmp) / rel, out)
            print(f"updated {out.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
