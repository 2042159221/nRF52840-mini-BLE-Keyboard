from pathlib import Path

from grpc_tools import protoc


def main() -> int:
    proto_dir = Path(__file__).resolve().parent
    proto_file = proto_dir / "device_comm.proto"
    return protoc.main(
        [
            "grpc_tools.protoc",
            f"-I{proto_dir}",
            f"--python_out={proto_dir}",
            str(proto_file),
        ]
    )


if __name__ == "__main__":
    raise SystemExit(main())
