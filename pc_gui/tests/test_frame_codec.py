import pytest

from pc_gui.device.error_codes import FrameDecodeError
from pc_gui.device.frame_codec import FrameCodec, FrameStreamDecoder, MAX_PAYLOAD


def test_frame_round_trip():
    payload = b"\x01\x02\x03"
    frame = FrameCodec.encode(payload)
    assert frame[0:2] == b"\x55\xaa"
    assert FrameCodec.decode(frame) == payload


def test_bad_magic_and_crc_are_rejected():
    frame = bytearray(FrameCodec.encode(b"\x01"))
    frame[0] = 0
    with pytest.raises(FrameDecodeError):
        FrameCodec.decode(bytes(frame))

    frame = bytearray(FrameCodec.encode(b"\x01"))
    frame[-1] ^= 0x55
    with pytest.raises(FrameDecodeError):
        FrameCodec.decode(bytes(frame))


def test_partial_and_oversized_frames_are_rejected():
    frame = FrameCodec.encode(b"\x01\x02")
    with pytest.raises(FrameDecodeError):
        FrameCodec.decode(frame[:-1])

    with pytest.raises(ValueError):
        FrameCodec.encode(bytes(MAX_PAYLOAD + 1))


def test_stream_decoder_handles_multiple_frames_and_noise():
    decoder = FrameStreamDecoder()
    first = FrameCodec.encode(b"one")
    second = FrameCodec.encode(b"two")
    assert decoder.feed(b"noise" + first[:2]) == []
    assert decoder.feed(first[2:] + second) == [b"one", b"two"]
