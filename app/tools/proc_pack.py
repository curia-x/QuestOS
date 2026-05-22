#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0

"""Build process package images from a TOML description."""

# package format:
#   header
#   desc table
#   image 0 payload
#   image 0 meta
#   image 1 payload
#   image 1 meta
#   ...

# meta format:
#   proc_image_meta
#   argv offset table      # u32[], offset relative to string table
#   env offset table       # u32[], offset relative to string table
#   auxv table             # { u64 type, u64 value }[]
#   string table           # argv/env 字符串，以 '\0' 结尾


import argparse
import struct
from pathlib import Path

try:
    import tomllib
except ModuleNotFoundError:
    import tomli as tomllib


PROC_PKG_MAGIC = 0x504B4750
PROC_PKG_VERSION = 1

PROC_IMAGE_TYPE_ELF = 1
PROC_IMAGE_TYPE_FLAT = 2

PROC_IMAGE_F_USER = 1 << 0
PROC_IMAGE_F_EXEC = 1 << 1

IMAGE_TYPES = {
    "elf": PROC_IMAGE_TYPE_ELF,
    "flat": PROC_IMAGE_TYPE_FLAT,
    "bin": PROC_IMAGE_TYPE_FLAT,
    "raw": PROC_IMAGE_TYPE_FLAT,
}

IMAGE_FLAGS = {
    "user": PROC_IMAGE_F_USER,
    "exec": PROC_IMAGE_F_EXEC,
}

AUXV_TYPES = {
    "AT_NULL": 0,
    "AT_IGNORE": 1,
    "AT_EXECFD": 2,
    "AT_PHDR": 3,
    "AT_PHENT": 4,
    "AT_PHNUM": 5,
    "AT_PAGESZ": 6,
    "AT_BASE": 7,
    "AT_FLAGS": 8,
    "AT_ENTRY": 9,
    "AT_NOTELF": 10,
    "AT_UID": 11,
    "AT_EUID": 12,
    "AT_GID": 13,
    "AT_EGID": 14,
    "AT_HWCAP": 16,
    "AT_CLKTCK": 17,
    "AT_RANDOM": 25,
    "AT_EXECFN": 31,
    "AT_SYSINFO_EHDR": 33,
}

# struct proc_pkg_header {
#     u32 magic;
#     u16 version;
#     u16 header_size;
#     u32 image_count;
#     u32 desc_offset;
#     u32 desc_size;
#     u32 reserved;
#     u64 total_size;
# };
HEADER_FMT = "<IHHIIIIQ"

# struct proc_image_desc {
#     u32 type;
#     u32 flags;
#     u64 image_offset;
#     u64 image_size;
#     u64 meta_offset;
#     u64 meta_size;
#     u64 load_hint;
#     u64 entry_hint;
# };
DESC_FMT = "<IIQQQQQQ"

# struct proc_image_meta {
#     u32 argv_count;
#     u32 env_count;
#     u32 auxv_count;
#     u32 string_table_size;
#     u32 name_offset;
#     u32 argv_offset;
#     u32 env_offset;
#     u32 auxv_offset;
#     u32 string_table_offset;
#     u32 reserved;
#     u64 stack_size;
# };
META_FMT = "<IIIIIIIIIIQ"

# auxv entry in meta blob:
#     u64 type;
#     u64 value;
AUXV_ENTRY_FMT = "<QQ"

# argv/env table entry:
#     u32 string_offset;
STRING_REF_FMT = "<I"

HEADER_SIZE = struct.calcsize(HEADER_FMT)
DESC_SIZE = struct.calcsize(DESC_FMT)
META_SIZE = struct.calcsize(META_FMT)
AUXV_ENTRY_SIZE = struct.calcsize(AUXV_ENTRY_FMT)
STRING_REF_SIZE = struct.calcsize(STRING_REF_FMT)


def align_up(value: int, align: int) -> int:
    """Round value up to the next multiple of align."""
    return (value + align - 1) & ~(align - 1)


def detect_image_type(data: bytes) -> int:
    """Return the process image type inferred from the image data."""
    if data.startswith(b"\x7fELF"):
        return PROC_IMAGE_TYPE_ELF
    return PROC_IMAGE_TYPE_FLAT


def write_padding(f, current: int, target: int):
    """Write zero bytes to pad the file from current to target offset."""
    if target < current:
        raise RuntimeError("internal offset error")

    f.write(b"\x00" * (target - current))


def main():
    """Pack multiple images into a single process package file."""
    args = _parse_args()

    images = _load_images(args.config_file)

    for img in images:
        img["meta"] = _build_image_meta(img)

    total_size = _layout_package(images, args.align)
    descs = _build_descriptors(images)

    _write_package(args.output, descs, images, total_size)
    _print_summary(args.output, images, total_size)


def _parse_args():
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--config-file",
        default="app/config/app-config.toml",
        help="path to TOML package config",
    )
    parser.add_argument("-o", "--output", required=True)
    parser.add_argument("--align", type=int, default=4096)
    args = parser.parse_args()

    if args.align <= 0 or (args.align & (args.align - 1)) != 0:
        raise SystemExit("--align must be power of two")

    return args


def _parse_u64(value, field, image_name=None):
    prefix = f"{image_name}: " if image_name else ""

    if isinstance(value, bool):
        raise TypeError(f"{prefix}{field} must be an integer")

    if isinstance(value, int):
        num = value
    elif isinstance(value, str):
        num = int(value, 0)
    else:
        raise TypeError(f"{prefix}{field} must be an integer")

    if num < 0 or num > 0xffffffffffffffff:
        raise ValueError(f"{prefix}{field} out of u64 range: {num}")

    return num


def _parse_str_list(value, field, image_name):
    if value is None:
        return []

    if not isinstance(value, list):
        raise TypeError(f"{image_name}: {field} must be a list")

    for item in value:
        if not isinstance(item, str):
            raise TypeError(f"{image_name}: {field} item must be string")

    return value


def _parse_image_type(value, image_name):
    if isinstance(value, int):
        return value

    if not isinstance(value, str):
        raise TypeError(f"{image_name}: type must be string or integer")

    key = value.lower()

    if key not in IMAGE_TYPES:
        raise ValueError(f"{image_name}: unknown image type: {value}")

    return IMAGE_TYPES[key]


def _parse_flags(value, image_name):
    if value is None:
        return 0

    if isinstance(value, int):
        return value

    if isinstance(value, str):
        value = [value]

    if not isinstance(value, list):
        raise TypeError(f"{image_name}: flags must be integer, string, or list")

    flags = 0

    for item in value:
        if not isinstance(item, str):
            raise TypeError(f"{image_name}: flag item must be string")

        key = item.lower()

        if key not in IMAGE_FLAGS:
            raise ValueError(f"{image_name}: unknown flag: {item}")

        flags |= IMAGE_FLAGS[key]

    return flags


def _parse_auxv_type(value, image_name):
    if isinstance(value, int):
        return value

    if not isinstance(value, str):
        raise TypeError(f"{image_name}: auxv type must be string or integer")

    if value in AUXV_TYPES:
        return AUXV_TYPES[value]

    return int(value, 0)


def _parse_auxv(value, image_name):
    if value is None:
        return []

    auxv = []

    # TOML style:
    #
    # [images.auxv]
    # AT_PAGESZ = 4096
    # AT_CLKTCK = 100
    if isinstance(value, dict):
        for aux_type, aux_value in value.items():
            auxv.append((
                _parse_auxv_type(aux_type, image_name),
                _parse_u64(aux_value, f"auxv.{aux_type}", image_name),
            ))
        return auxv

    # TOML style:
    #
    # [[images.auxv]]
    # type = "AT_PAGESZ"
    # value = 4096
    if isinstance(value, list):
        for index, item in enumerate(value):
            if not isinstance(item, dict):
                raise TypeError(f"{image_name}: auxv[{index}] must be a table")

            if "type" not in item or "value" not in item:
                raise ValueError(
                    f"{image_name}: auxv[{index}] requires type and value"
                )

            auxv.append((
                _parse_auxv_type(item["type"], image_name),
                _parse_u64(item["value"], f"auxv[{index}].value", image_name),
            ))

        return auxv

    raise TypeError(f"{image_name}: auxv must be table or table array")


def _load_images(config_path):
    """Load image pack inputs and binary payloads from a TOML config file.

    This loads the fields known before package layout: type, flags, path,
    payload data, argv/env/auxv, stack size, and flat-binary load/entry hints.

    Package offsets are computed later.
    """
    config_path = Path(config_path).resolve()
    config_dir = config_path.parent

    with open(config_path, "rb") as f:
        cfg = tomllib.load(f)

    image_dir = Path(cfg.get("image_dir", "."))

    if not image_dir.is_absolute():
        image_dir = config_dir / image_dir

    image_dir = image_dir.resolve()

    default_flags = cfg.get("default_flags", ["user", "exec"])
    default_stack_size = _parse_u64(
        cfg.get("default_stack_size", 64 * 1024),
        "default_stack_size",
    )

    if "images" not in cfg:
        raise ValueError("missing [[images]] section")

    images = []

    for index, item in enumerate(cfg["images"]):
        if not isinstance(item, dict):
            raise TypeError(f"images[{index}] must be a table")

        if "name" not in item:
            raise ValueError(f"images[{index}] missing name")

        name = item["name"]

        if not isinstance(name, str):
            raise TypeError(f"images[{index}].name must be string")

        filename = item.get("file", name + ".elf")

        if not isinstance(filename, str):
            raise TypeError(f"{name}: file must be string")

        file_path = Path(filename)

        if file_path.is_absolute():
            image_path = file_path.resolve()
        else:
            image_path = (image_dir / file_path).resolve()

        if not image_path.is_file():
            raise FileNotFoundError(f"{name}: image file not found: {image_path}")

        data = image_path.read_bytes()

        image_type = _parse_image_type(
            item.get("type", detect_image_type(data)),
            name,
        )

        flags = _parse_flags(item.get("flags", default_flags), name)

        stack_size = _parse_u64(
            item.get("stack_size", default_stack_size),
            "stack_size",
            name,
        )

        load_hint = _parse_u64(
            item.get("load_hint", 0),
            "load_hint",
            name,
        )

        entry_hint = _parse_u64(
            item.get("entry_hint", 0),
            "entry_hint",
            name,
        )

        if image_type == PROC_IMAGE_TYPE_FLAT:
            if load_hint == 0 or entry_hint == 0:
                raise ValueError(
                    f"{name}: flat image requires non-zero load_hint and entry_hint"
                )

        if "argv" in item:
            argv = _parse_str_list(item["argv"], "argv", name)
        else:
            argv0 = item.get("argv0", Path(filename).name)

            if not isinstance(argv0, str):
                raise TypeError(f"{name}: argv0 must be string")

            args = _parse_str_list(item.get("args", []), "args", name)
            argv = [argv0] + args

        env = _parse_str_list(item.get("env", []), "env", name)

        auxv = []
        auxv.extend(_parse_auxv(item.get("auxv", None), name))
        auxv.extend(_parse_auxv(item.get("auxv_extra", None), name))

        # Store a complete auxv table in metadata.
        # If the kernel wants to append AT_NULL itself, remove this block.
        if not auxv or auxv[-1][0] != AUXV_TYPES["AT_NULL"]:
            auxv.append((AUXV_TYPES["AT_NULL"], 0))

        images.append({
            "name": name,
            "path": str(image_path),

            "type": image_type,
            "flags": flags,
            "load_hint": load_hint,
            "entry_hint": entry_hint,

            "data": data,
            "image_size": len(data),

            "argv": argv,
            "env": env,
            "auxv": auxv,
            "stack_size": stack_size,

            "image_offset": 0,
            "meta_offset": 0,
            "meta_size": 0,
        })

    return images


def _append_c_string(string_table: bytearray, value: str) -> int:
    offset = len(string_table)
    string_table.extend(value.encode("utf-8"))
    string_table.append(0)
    return offset


def _build_image_meta(img):
    """Build proc_image_meta blob for one image.

    argv/env tables contain u32 offsets relative to string_table_offset.
    auxv table contains pairs of u64 type/value.
    """
    string_table = bytearray()

    name_offset = _append_c_string(string_table, img["name"])

    argv_string_offsets = [
        _append_c_string(string_table, value)
        for value in img["argv"]
    ]

    env_string_offsets = [
        _append_c_string(string_table, value)
        for value in img["env"]
    ]

    string_table_size = len(string_table)

    argv_offset = META_SIZE
    env_offset = argv_offset + len(argv_string_offsets) * STRING_REF_SIZE
    auxv_offset = align_up(env_offset + len(env_string_offsets) * STRING_REF_SIZE, 8)
    string_table_offset = auxv_offset + len(img["auxv"]) * AUXV_ENTRY_SIZE

    meta = bytearray()
    meta.extend(b"\x00" * META_SIZE)

    for off in argv_string_offsets:
        meta.extend(struct.pack(STRING_REF_FMT, off))

    for off in env_string_offsets:
        meta.extend(struct.pack(STRING_REF_FMT, off))

    while len(meta) < auxv_offset:
        meta.append(0)

    for aux_type, aux_value in img["auxv"]:
        meta.extend(struct.pack(AUXV_ENTRY_FMT, aux_type, aux_value))

    if len(meta) != string_table_offset:
        raise RuntimeError(f"{img['name']}: internal meta layout error")

    meta.extend(string_table)

    meta_header = struct.pack(
        META_FMT,
        len(img["argv"]),
        len(img["env"]),
        len(img["auxv"]),
        string_table_size,
        name_offset,
        argv_offset,
        env_offset,
        auxv_offset,
        string_table_offset,
        0,
        img["stack_size"],
    )

    meta[:META_SIZE] = meta_header

    return bytes(meta)


def _layout_package(images, align):
    """Compute image/meta offsets in the final package."""
    desc_offset = HEADER_SIZE
    desc_size = len(images) * DESC_SIZE

    current = align_up(desc_offset + desc_size, align)

    for img in images:
        current = align_up(current, align)
        img["image_offset"] = current
        img["image_size"] = len(img["data"])
        current += img["image_size"]

        current = align_up(current, align)
        img["meta_offset"] = current
        img["meta_size"] = len(img["meta"])
        current += img["meta_size"]

    return align_up(current, align)


def _build_descriptors(images):
    """Build packed proc_image_desc entries."""
    descs = []

    for img in images:
        descs.append(struct.pack(
            DESC_FMT,
            img["type"],
            img["flags"],
            img["image_offset"],
            img["image_size"],
            img["meta_offset"],
            img["meta_size"],
            img["load_hint"],
            img["entry_hint"],
        ))

    return descs


def _write_package(output_path, descs, images, total_size):
    """Write the process package to file."""
    header = struct.pack(
        HEADER_FMT,
        PROC_PKG_MAGIC,
        PROC_PKG_VERSION,
        HEADER_SIZE,
        len(images),
        HEADER_SIZE,
        len(descs) * DESC_SIZE,
        0,
        total_size,
    )

    out_path = Path(output_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    with out_path.open("wb") as f:
        f.write(header)

        for desc in descs:
            f.write(desc)

        for img in images:
            pos = f.tell()
            write_padding(f, pos, img["image_offset"])
            f.write(img["data"])

            pos = f.tell()
            write_padding(f, pos, img["meta_offset"])
            f.write(img["meta"])

        pos = f.tell()
        write_padding(f, pos, total_size)


def _print_summary(output_path, images, total_size):
    """Print package summary."""
    print(f"PY\tcreated {Path(output_path)}")
    print(f"PY\t  header_size = {HEADER_SIZE}")
    print(f"PY\t  desc_size   = {DESC_SIZE}")
    print(f"PY\t  meta_size   = {META_SIZE}")
    print(f"PY\t  image_count = {len(images)}")
    print(f"PY\t  total_size  = {total_size}")

    for idx, img in enumerate(images):
        print(
            "PY\t"
            f"[{idx}] {img['name']} "
            f"type={img['type']} "
            f"flags=0x{img['flags']:x} "
            f"image=0x{img['image_offset']:x}+0x{img['image_size']:x} "
            f"meta=0x{img['meta_offset']:x}+0x{img['meta_size']:x} "
            f"path={img['path']}"
        )


if __name__ == "__main__":
    main()
