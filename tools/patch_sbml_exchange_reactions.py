#!/usr/bin/env python3

from __future__ import annotations

import argparse
import copy
import sys
import xml.etree.ElementTree as ET


def local_name(tag: str) -> str:
    if tag.startswith("{"):
        return tag.split("}", 1)[1]
    return tag


def patch_reactions(root: ET.Element) -> tuple[int, int, int]:
    patched = 0
    missing_reversible = 0
    missing_fast = 0
    for reaction in root.iter():
        if local_name(reaction.tag) != "reaction":
            continue

        changed = False
        if "reversible" not in reaction.attrib:
            reaction.set("reversible", "false")
            missing_reversible += 1
            changed = True
        if "fast" not in reaction.attrib:
            reaction.set("fast", "false")
            missing_fast += 1
            changed = True
        if changed:
            patched += 1

    return patched, missing_reversible, missing_fast


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Add missing SBML L3V1 reaction attributes to all reactions."
    )
    parser.add_argument("input_xml", help="Path to the input SBML file")
    parser.add_argument(
        "output_xml",
        nargs="?",
        help="Path to the output SBML file. Defaults to overwriting the input file.",
    )
    args = parser.parse_args()

    input_path = args.input_xml
    output_path = args.output_xml or args.input_xml

    tree = ET.parse(input_path)
    root = tree.getroot()

    patched, missing_reversible, missing_fast = patch_reactions(root)

    if output_path == input_path:
        backup_path = input_path + ".bak"
        with open(input_path, "rb") as source, open(backup_path, "wb") as backup:
            backup.write(source.read())
        print(f"Backup written to {backup_path}")

    tree.write(output_path, encoding="utf-8", xml_declaration=True)
    print(
        f"Patched {patched} reaction(s) in {output_path} "
        f"(added reversible to {missing_reversible}, fast to {missing_fast})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
