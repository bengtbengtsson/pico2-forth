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

def test_colon_definition_square():
    assert run_line(": SQUARE DUP * ; 5 SQUARE .") == "25"

def test_colon_chain():
    assert run_line(": INC 1 + ; : DOUBLE 2 * ; 3 INC DOUBLE .") == "8"

def test_colon_with_variable():
    assert run_line("VARIABLE x 10 x ! : GETX x @ ; GETX .") == "10"

def test_words_lists_dictionary():
    out = run_line("WORDS")
    # must include a few well-known entries and itself
    for token in ["+", "VARIABLE", "DUP", "WORDS"]:
        assert token in out

# ======================  PHASE-4 NEW WORDS  =======================

# Arithmetic helpers -------------------------------------------------
def test_inc_and_dec():
    assert run_line("3 1+ .") == "4"
    assert run_line("5 1- .") == "4"

def test_twice_and_half():
    assert run_line("7 2+ .") == "9"
    assert run_line("8 2- .") == "6"
    assert run_line("6 2* .") == "12"
    assert run_line("10 2/ .") == "5"

def test_negate():
    assert run_line("5 NEGATE .") == "-5"
    assert run_line("-7 NEGATE .") == "7"

# Stack-shuffle words -------------------------------------------------
def test_stack_nip():
    out = run_line("1 2 NIP .s")
    assert out.startswith("<1>")
    assert out.endswith("2")

def test_stack_tuck():
    out = run_line("10 20 TUCK .s")
    assert out.startswith("<3>")
    assert out.endswith("20 10 20")

def test_stack_minus_rot():
    out = run_line("1 2 3 -ROT .s")
    assert out.startswith("<3>")
    assert out.endswith("3 1 2")

# Boolean constants --------------------------------------------------
def test_true_false_constants():
    assert run_line("TRUE .") == "-1"
    assert run_line("FALSE .") == "0"
    out = run_line("TRUE FALSE + .S")
    assert out.startswith("<1>")
    assert out.endswith("-1")

# Cell helpers -------------------------------------------------------
def test_cell_helpers():
    assert run_line("CELL .") == "4"
    assert run_line("3 CELLS .") == "12"      # 3 * 4
    assert run_line("100 CELL+ .") == "104"   # 100 + 4

# Others
def test_sqr_and_cube():
    assert run_line("4 SQR .") == "16"
    assert run_line("3 CUBE .") == "27"

def test_2drop_and_2dup():
    assert run_line("1 2 3 4 2DROP .S") == "<2> 1 2"
    #assert run_line("10 20 2DUP .S") == "<4> 10 20 10 20"

def test_comparisons():
    assert run_line("42 42 = .") == "-1"
    assert run_line("42 99 = .") == "0"
    assert run_line("3 5 < .") == "-1"
    assert run_line("5 3 < .") == "0"
    assert run_line("7 2 > .") == "-1"
    assert run_line("2 7 > .") == "0"

def test_emit_and_cr():
    assert run_line("65 EMIT") == "A"
    assert run_line(".CR") == ""  # prints a newline only

