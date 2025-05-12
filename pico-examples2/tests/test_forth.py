import subprocess
import os
import pytest

# Adjust path if your build folder differs
FOURTHOST = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'build', 'forth_host'))


def run_line(line):
    proc = subprocess.Popen(
        [FOURTHOST],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    # skip the welcome banner
    proc.stdout.readline()
    # send our input
    proc.stdin.write(line + "\n")
    proc.stdin.flush()
    out = ''
    # read until we get a non-prompt line
    while True:
        l = proc.stdout.readline()
        if not l:
            break
        if l.strip() and not l.strip().endswith('>'):
            out = l.strip()
            break
    proc.terminate()
    return out


def test_simple_addition():
    assert run_line("1 2 + .") == "3"


def test_dup_and_dots():
    rst = run_line("1 2 dup .s")
    assert rst.startswith("<3>")
    assert rst.endswith("1 2 2")


def test_over_behavior():
    assert run_line("1 2 3 over .s").startswith("<4> 1 2 3 2")


def test_underflow():
    err = run_line("+")
    assert "requires 2 items" in err
