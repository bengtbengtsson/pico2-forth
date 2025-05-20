import os
import pty
import subprocess
import pytest
import time
import select

FOURTHOST = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'build-host', 'forth_host'))

pytestmark = pytest.mark.skipif(
    not os.path.exists(FOURTHOST),
    reason="forth_host binary not found"
)

def run_line(line):
    master_fd, slave_fd = pty.openpty()

    proc = subprocess.Popen(
        [FOURTHOST],
        stdin=slave_fd,
        stdout=slave_fd,
        stderr=slave_fd,
        text=True,
        close_fds=True
    )

    os.write(master_fd, (line + "\n").encode())

    output = []
    timeout = 1.0
    start = time.time()
    ok_prompt_count = 0

    while time.time() - start < timeout:
        rlist, _, _ = select.select([master_fd], [], [], 0.1)
        if master_fd in rlist:
            try:
                data = os.read(master_fd, 1024).decode(errors="ignore")
                output.append(data)
                ok_prompt_count += data.count("ok>")
                if ok_prompt_count >= 2:
                    break
            except OSError:
                break

    os.close(master_fd)
    os.close(slave_fd)
    proc.kill()

    full_output = "".join(output)
    lines = [l.strip() for l in full_output.splitlines() if l.strip()]
    cleaned = [l for l in lines if not l.startswith("ok>") and l != line and not l.startswith("Simple Forth")]

    return "\n".join(cleaned) if cleaned else ""


   # return cleaned[-1] if cleaned else ""

# ========================== TESTS ===============================

def test_simple_addition():
    assert run_line("1 2 + .") == "3"
    assert run_line("0 2 + .") == "2"
    assert run_line("0 0 + .") == "0"
    assert run_line("-1 0 + .") == "-1"
    assert run_line("-1 -1 + .") == "-2"

def test_simple_subtraction():
    assert run_line("1 2 - .") == "-1"
    assert run_line("0 2 - .") == "-2"
    assert run_line("0 0 - .") == "0"
    assert run_line("-1 0 - .") == "-1"
    assert run_line("-1 -1 - .") == "0"

def test_simple_multiplication():
    assert run_line("1 2 * .") == "2"
    assert run_line("0 2 * .") == "0"
    assert run_line("0 0 * .") == "0"
    assert run_line("-1 0 * .") == "0"
    assert run_line("-1 -1 * .") == "1"

def test_simple_division():
    assert run_line("1 2 / .") == "0"
    assert run_line("0 2 / .") == "0"
    assert run_line("0 0 /") == "Error: division by zero"
    assert run_line("-1 0 /") == "Error: division by zero"
    assert run_line("-1 -1 / .") == "1"

def test_dup_and_dots():
    output = run_line("1 2 DUP .s")
    print(f"[TEST OUTPUT] {output!r}")
    assert output.startswith("<3>")
    assert output.endswith("1 2 2")

def test_over_behavior():
    output = run_line("1 2 3 OVER .s")
    print(f"[TEST OUTPUT] {output!r}")
    assert output.startswith("<4>")
    assert output.endswith("1 2 3 2")

def test_underflow():
    err = run_line("+")
    assert "requires 2 items" in err

# Memory tests (! and @)
def test_store_and_fetch():
    assert run_line("42 100 ! 100 @ .") == "42"

def test_overwrite_store():
    assert run_line("1 200 ! 2 200 ! 200 @ .") == "2"

def test_fetch_default():
    assert run_line("300 @ .") == "0"

def test_store_negative_address():
    err = run_line("123 -1 !")
    assert "invalid store address" in err

def test_fetch_invalid_address():
    err = run_line("-5 @")
    assert "invalid fetch address" in err

def test_store_underflow():
    err = run_line("42 !")
    assert "requires 2 items" in err

def test_fetch_underflow():
    err = run_line("@")
    assert "requires 1 item" in err

# Variable and Constant tests
def test_constant_definition():
    assert run_line("99 CONSTANT answer answer .") == "99"

def test_variable_definition_and_store():
    assert run_line("VARIABLE x 123 x ! x @ .") == "123"

def test_variable_multiple():
    assert run_line("VARIABLE a VARIABLE b 77 a ! 88 b ! a @ .") == "77"

def test_mod_behavior():
    assert run_line("10 3 MOD .") == "1"
    assert run_line("10 2 MOD .") == "0"
    assert run_line("10 1 MOD .") == "0"
    assert run_line("5 0 MOD") == "Error: division by zero"

def test_divmod_behavior():
    assert run_line("10 3 /MOD . .") == "3\n1"
    assert run_line("10 2 /MOD . .") == "5\n0"
    assert run_line("10 1 /MOD . .") == "10\n0"
    assert run_line("5 0 /MOD") == "Error: division by zero"

def test_mod_signed_behavior():
    """Remainder must have the sign of the divisor (ANS floored semantics)."""
    assert run_line("-10 3 MOD .") == "2"      #  -10 = 3 * (-4) + 2
    assert run_line("10 -3 MOD .") == "-2"     #   10 = (-3) * (-4) + (-2)
    assert run_line("-10 -3 MOD .") == "-1"    #  -10 = (-3) * 3 + (-1)
    assert run_line("-9 3 MOD .") == "0"
    assert run_line("9 -3 MOD .") == "0"

def test_divmod_signed_behavior():
    """
    /MOD leaves remainder (lower) then quotient (TOS).
    With . . we therefore expect "quotient\\nremainder".
    """
    assert run_line("-10 3 /MOD . .") == "-4\n2"    # q=-4, r=2
    assert run_line("10 -3 /MOD . .") == "-4\n-2"   # q=-4, r=-2
    assert run_line("-10 -3 /MOD . .") == "3\n-1"   # q=3,  r=-1
    assert run_line("-9 3 /MOD . .") == "-3\n0"
    assert run_line("9 -3 /MOD . .") == "-3\n0"
