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
    timeout = 1.0  # max read time
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
                    break  # assume prompt appears twice: startup + after response
            except OSError:
                break

    os.close(master_fd)
    os.close(slave_fd)
    proc.kill()

    full_output = "".join(output)
    print(f"\n[DEBUG] Input: {line}")
    print("[DEBUG] Full PTY output:")
    print(full_output)

    # Extract last meaningful line
    lines = [l.strip() for l in full_output.splitlines() if l.strip()]
    cleaned = [l for l in lines if not l.startswith("ok>") and l != line and not l.startswith("Simple Forth")]

    return cleaned[-1] if cleaned else ""

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

