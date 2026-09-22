"""Protocol regression tests for the extended SICP framing.

Verified TX framing (proven on hardware for power commands):
    A6 01 00 00 00 | SIZE | 01 | CMD... | CHECKSUM
    SIZE = len(cmd) + 2
    CHECKSUM = 0xA7 ^ SIZE ^ 0x01 ^ each command byte

Run: python3 -m pytest tests/ -v
"""


def extended_checksum(cmd: bytes) -> int:
    c = 0xA7
    c ^= len(cmd) + 2
    c ^= 0x01
    for b in cmd:
        c ^= b
    return c


def build_packet(cmd: bytes) -> bytes:
    size = len(cmd) + 2
    return bytes([0xA6, 0x01, 0x00, 0x00, 0x00, size, 0x01, *cmd, extended_checksum(cmd)])


def test_power_on_vector():
    assert build_packet(bytes([0x18, 0x02])).hex(" ").upper() == "A6 01 00 00 00 04 01 18 02 B8"


def test_power_off_vector():
    assert build_packet(bytes([0x18, 0x01])).hex(" ").upper() == "A6 01 00 00 00 04 01 18 01 BB"


def test_volume_50_vector():
    # Cross-checked against a known-good legacy packet.
    assert build_packet(bytes([0x44, 0x32])).hex(" ").upper() == "A6 01 00 00 00 04 01 44 32 D4"


def test_hdmi_vector():
    # The checksum algorithm that matches every other proven vector
    # (power on/off, volume) yields 0x08 for this payload, not the 0x0F
    # recorded by a legacy implementation:
    #   0xA7 ^ 0x07 ^ 0x01 ^ AC ^ 06 ^ 02 ^ 01 ^ 00 = 0x08
    # Hardware test confirmed the 0x08 variant is accepted (SET success
    # reply observed), so the builder follows the algorithm.
    pkt = build_packet(bytes([0xAC, 0x06, 0x02, 0x01, 0x00]))
    assert pkt.hex(" ").upper() == "A6 01 00 00 00 07 01 AC 06 02 01 00 08"


def test_checksum_boundaries():
    assert extended_checksum(bytes([0x18, 0x02])) == 0xB8
    assert extended_checksum(bytes([0x44, 0x64])) == 0xA7 ^ 0x04 ^ 0x01 ^ 0x44 ^ 0x64
    # empty command still frames (not used in practice)
    assert build_packet(b"")[0:6] == bytes.fromhex("A6 01 00 00 00 02")


def test_generic_sicp_checksum_example():
    # Sanity check for the *documented generic* SICP checksum
    # (XOR of all bytes except checksum itself), used for RX fallback parsing.
    # Doc example: power-state report on addr 01: 05 01 19 02 1F
    frame = bytes([0x05, 0x01, 0x19, 0x02])
    calc = 0
    for b in frame:
        calc ^= b
    assert calc == 0x1F


def rx_parse(frame: bytes) -> bytes:
    """Mirror of the C++ extended RX parser. Returns the CMD payload."""
    assert len(frame) >= 7
    magic = frame[0]
    assert magic in (0xA6, 0x21)
    assert frame[1:4] == bytes([0x01, 0x00, 0x00])
    if magic == 0xA6:
        assert frame[4] == 0x00
        hdr = 5
    else:
        hdr = 4
    size = frame[hdr]
    assert len(frame) == size + hdr + 1
    assert frame[hdr + 1] == 0x01
    calc = 0
    for b in frame[:-1]:
        calc ^= b
    assert calc == frame[-1], f"checksum: got {frame[-1]:02X} want {calc:02X}"
    return frame[hdr + 2 : -1]


def test_tx_frame_layout():
    # TX has one extra 0x00 header byte vs RX.
    tx = build_packet(bytes([0x18, 0x02]))
    assert tx[:6] == bytes.fromhex("A6 01 00 00 00 04")
    assert len(tx) == 4 + 6  # SIZE + hdr(5) + 1


def test_rx_volume_report():
    # Captured from hardware: TX volume GET, RX volume 50 (0x32).
    assert rx_parse(bytes.fromhex("21 01 00 00 04 01 45 32 52")) == bytes([0x45, 0x32])


def test_rx_picture_format_report():
    # Captured: picture format 0x00 (Normal).
    assert rx_parse(bytes.fromhex("21 01 00 00 04 01 3B 00 1E")) == bytes([0x3B, 0x00])


def test_rx_video_report():
    # Captured: brightness=100, contrast=50, sharpness=50.
    assert rx_parse(bytes.fromhex("21 01 00 00 08 01 33 64 00 32 32 00 7E")) == bytes(
        [0x33, 0x64, 0x00, 0x32, 0x32, 0x00]
    )


def test_rx_operating_hours_report():
    # Captured: 0x0F 0x64 0x7B -> 0x647B = 25723 h.
    payload = rx_parse(bytes.fromhex("21 01 00 00 05 01 0F 64 7B 34"))
    assert payload == bytes([0x0F, 0x64, 0x7B])
    assert (payload[1] << 8) | payload[2] == 25723


def test_tx_checksum_equals_xor_with_header():
    # The 0xA7-based TX formula is exactly XOR over the whole TX frame
    # except the checksum, because A6^01^00^00 == A7.
    for cmd in (bytes([0x18, 0x02]), bytes([0x45]), bytes([0x0F, 0x02])):
        pkt = build_packet(cmd)
        calc = 0
        for b in pkt[:-1]:
            calc ^= b
        assert calc == pkt[-1]


def classify_comm_control(value: int) -> str:
    """Mirror of PhilipsSicp::on_ack_report_ classification."""
    if value in (0x06, 0x00):
        return "ack"
    if value == 0x15:
        return "nack"
    if value == 0x18:
        return "nav"
    return "unknown-terminal"


def test_rx_set_success_reply():
    # Captured from hardware: volume SET and input SET are both answered
    # with 00 00 (the documented ACK value 00 06 was never observed).
    assert rx_parse(bytes.fromhex("21 01 00 00 04 01 00 00 25")) == bytes([0x00, 0x00])
    assert classify_comm_control(0x00) == "ack"
    assert classify_comm_control(0x06) == "ack"
    assert classify_comm_control(0x15) == "nack"
    assert classify_comm_control(0x18) == "nav"


def test_rx_unsupported_command_reply():
    # Captured from hardware: temperature GET on a display without that
    # sensor is answered with 00 03 (undocumented).
    assert rx_parse(bytes.fromhex("21 01 00 00 04 01 00 03 26")) == bytes([0x00, 0x03])
    assert classify_comm_control(0x03) == "unknown-terminal"


def test_extended_command_vectors():
    # Documentation-derived vectors; checksum follows the same algorithm.
    vectors = {
        (0xA2, 0x00): "A6 01 00 00 00 04 01 A2 00 00",
        (0xA2, 0x01): "A6 01 00 00 00 04 01 A2 01 01",
        (0x1D,): "A6 01 00 00 00 03 01 1D B8",
        (0x1C, 0x03): "A6 01 00 00 00 04 01 1C 03 BD",
        (0xA3, 0x02): "A6 01 00 00 00 04 01 A3 02 03",
        (0xB8, 0x0A, 0x4D, 0x32): "A6 01 00 00 00 06 01 B8 0A 4D 32 6D",
        (0x43,): "A6 01 00 00 00 03 01 43 E6",
        (0x42, 0x4D, 0x4D): "A6 01 00 00 00 05 01 42 4D 4D E1",
        (0xDD, 0x02): "A6 01 00 00 00 04 01 DD 02 7D",
        (0x70, 0x40, 0x00): "A6 01 00 00 00 05 01 70 40 00 93",
        (0x15,): "A6 01 00 00 00 03 01 15 B0",
        (0x23,): "A6 01 00 00 00 03 01 23 86",
        (0x22, 0x01, 0x02, 0x00, 0x00): "A6 01 00 00 00 07 01 22 01 02 00 00 80",
        (0xAF,): "A6 01 00 00 00 03 01 AF 0A",
    }
    for cmd, expected in vectors.items():
        assert build_packet(bytes(cmd)).hex(" ").upper() == expected, cmd


def tiling_vh_to_code(v, h):
    assert 1 <= v <= 5 and 1 <= h <= 5
    return (v - 1) * 5 + (h - 1) + 1


def tiling_code_to_vh(code):
    assert 0x01 <= code <= 0x19
    idx = code - 1
    return idx // 5 + 1, idx % 5 + 1


def test_tiling_vh_packing():
    # Documented examples: V=2,H=3 -> 0x08; V=5,H=4 -> 0x18; V=5,H=5 -> 0x19.
    assert tiling_vh_to_code(2, 3) == 0x08
    assert tiling_vh_to_code(5, 4) == 0x18
    assert tiling_vh_to_code(5, 5) == 0x19
    assert tiling_vh_to_code(1, 1) == 0x01
    for code in range(0x01, 0x1A):
        v, h = tiling_code_to_vh(code)
        assert tiling_vh_to_code(v, h) == code


def clamp_volume_limits(mn, mx, sw):
    mn, mx, sw = max(0, min(100, mn)), max(0, min(100, mx)), max(0, min(100, sw))
    sw = max(mn, min(mx, sw))
    return mn, mx, sw


def test_volume_limits_rule():
    # Mirrors PhilipsSicp::request_volume_limit (min <= switch-on <= max).
    assert clamp_volume_limits(10, 77, 50) == (10, 77, 50)
    assert clamp_volume_limits(60, 77, 50) == (60, 77, 60)
    assert clamp_volume_limits(10, 40, 50) == (10, 40, 40)
