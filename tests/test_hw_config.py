import json
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "hw_config.py"
BOARDS = sorted(p for p in (ROOT / "app" / "boards").glob("*.json") if not p.name.endswith(".schema.json"))


def test_three_boards_are_defined():
    assert {b.stem for b in BOARDS} == {"h753", "f767", "f439", "f407", "f401", "f446", "f103", "g071"}


def test_schema_covers_every_profile_key():
    schema = json.loads((ROOT / "app" / "boards" / "board.schema.json").read_text(encoding="utf-8"))
    for board in BOARDS:
        assert set(json.loads(board.read_text(encoding="utf-8"))) <= set(schema["properties"])


@pytest.mark.parametrize("board", BOARDS, ids=lambda b: b.stem)
def test_profile_validates_and_generates_header(board, tmp_path):
    out = tmp_path / "hw_config.h"
    subprocess.run([sys.executable, str(SCRIPT), str(board), "--out", str(out)], check=True)
    text = out.read_text(encoding="utf-8")
    assert "HW_I2C_SCL B, 8" in text
    assert "HW_GPIO_LIST(X)" in text and "HW_BOARD_NAME" in text


def test_script_rejects_a_clash(tmp_path):
    bad = tmp_path / "bad.json"
    bad.write_text(json.dumps({"board": "x", "gpio": ["PB0"], "adc": ["PB0"]}), encoding="utf-8")
    res = subprocess.run([sys.executable, str(SCRIPT), str(bad)], capture_output=True, text=True)
    assert res.returncode != 0
    assert "PB0 is assigned twice" in res.stderr
