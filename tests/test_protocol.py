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
    # Legacy implementation recorded ... AC 06 02 01 00 0F for HDMI.
    # The checksum algorithm that matches every other proven vector
    # (power on/off, volume) yields 0x08 for this payload, not 0x0F:
    #   0xA7 ^ 0x07 ^ 0x01 ^ AC ^ 06 ^ 02 ^ 01 ^ 00 = 0x08
    # Both variants are exercised against hardware before release;
    # the builder must follow the algorithm (0x08).
    pkt = build_packet(bytes([0xAC, 0x06, 0x02, 0x01, 0x00]))
    assert pkt.hex(" ").upper() == "A6 01 00 00 00 07 01 AC 06 02 01 00 08"
    legacy = bytes.fromhex("A6 01 00 00 00 07 01 AC 06 02 01 00 0F")
    assert legacy[:7] == pkt[:7]
    assert legacy[7:12] == pkt[7:12]
    # checksum byte is the only difference; recorded here so the
    # discrepancy stays visible until hardware resolves it.
    assert legacy[12] == 0x0F
    assert pkt[12] == 0x08


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
