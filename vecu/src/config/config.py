import csv
from dataclasses import dataclass
from pathlib import Path


@dataclass
class ConfigRow:
    """Stores all relevant fields from a single row of the unified configuration."""

    label: str
    id_tester: int
    id_gateway: int
    sid: int
    did: int
    down_sampling: bool
    data_size_in_bytes: int
    start_bit: int
    length_in_bit: int
    data_type: str
    factor: float
    offset: float
    interpolate: bool
    car_name: str

    def __str__(self):
        return (
            f"label: {self.label}\n"
            f"id_tester: {self.id_tester}\n"
            f"id_gateway: {self.id_gateway}\n"
            f"sid: {self.sid}\n"
            f"did: {self.did}\n"
            f"down_sampling: {self.down_sampling}\n"
            f"data_size_in_bytes: {self.data_size_in_bytes}\n"
            f"start_bit: {self.start_bit}\n"
            f"length_in_bit: {self.length_in_bit}\n"
            f"data_type: {self.data_type}\n"
            f"factor: {self.factor}\n"
            f"offset: {self.offset}\n"
            f"interpolate: {self.interpolate}\n"
            f"car_name: {self.car_name}\n"
        )

    def __repr__(self):
        return str(self)


class Config:
    """Stores all relevant information loaded from the UdsConfig csv file."""

    def __init__(self, path_config_csv: str | Path):
        self.path = Path(path_config_csv)
        self.config_rows: list[ConfigRow] = []
        self._load_config(self.path)

    def __len__(self):
        return len(self.config_rows)

    def __iter__(self):
        return iter(self.config_rows)

    def __getitem__(self, key):
        return self.config_rows[key]

    def get_car_name(self):
        return self.config_rows[0].car_name if self.config_rows else "Unknown"

    def get_rows(self) -> list[ConfigRow]:
        return self.config_rows

    def get_gateway_id(self, id_tester: int) -> int | None:
        """Returns id_gateway for a given id_tester, or None if not found."""
        for row in self.config_rows:
            if row.id_tester == id_tester:
                return row.id_gateway
        return None

    def get_unique_isotp_addresses(self) -> list[tuple[int, int]]:
        """Returns all unique ISO-TP address pairs from the config."""
        address_pairs = {(row.id_tester, row.id_gateway) for row in self.config_rows}
        return sorted(address_pairs)

    def get_labels(self) -> set[str]:
        """Returns all labels defined in the config."""
        return {row.label for row in self.config_rows}

    def _load_config(self, path_config_csv: Path):
        with path_config_csv.open("r", encoding="utf-8-sig", newline="") as config_csv:
            reader = csv.DictReader(config_csv, skipinitialspace=True)
            if reader.fieldnames is None:
                return

            reader.fieldnames = [self._normalize_field_name(name) for name in reader.fieldnames]

            for row in reader:
                normalized = {
                    self._normalize_field_name(key): self._normalize_value(value)
                    for key, value in row.items()
                    if key is not None
                }
                if not any(normalized.values()):
                    continue
                self.config_rows.append(self._build_config_row(normalized))

    def _build_config_row(self, values: dict[str, str]) -> ConfigRow:
        start_bit_in_index_format, length_in_bit = self._prepare_values_from_dbc_field(
            values.get("strDbc", "7|8@0")
        )
        return ConfigRow(
            label=values.get("strLab", ""),
            id_tester=self._parse_int(values.get("numIdTstr")),
            id_gateway=self._parse_int(values.get("numIdGtwy")),
            sid=self._parse_int(values.get("numSid")),
            did=self._parse_int(values.get("numDid")),
            down_sampling=bool(self._parse_int(values.get("numDwnSamp"))),
            data_size_in_bytes=self._parse_int(values.get("numBytes"), default=1),
            start_bit=start_bit_in_index_format,
            length_in_bit=length_in_bit,
            data_type=values.get("strType", "uint"),
            factor=self._parse_float(values.get("numFac"), default=1.0),
            offset=self._parse_float(values.get("numOfs"), default=0.0),
            interpolate=bool(self._parse_int(values.get("stIntp"))),
            car_name=values.get("strName", "Unknown"),
        )

    def _prepare_values_from_dbc_field(self, dbc_field: str):
        start_bit_in_dbc_format, length_in_bit, byte_order = self._extract_values_from_dbc_field(
            dbc_field
        )
        start_bit_in_index_format = self._convert_start_from_dbc_to_index(
            start_bit_in_dbc_format, byte_order
        )
        return start_bit_in_index_format, int(length_in_bit)

    def _extract_values_from_dbc_field(self, dbc_value: str) -> tuple[int, int, int]:
        """ Extracts the different information from the DBC (Databse CAN) field.
        field structure: start_bit|length@byte_order
            - @0: Big Endian
            - @1: Little Endian
        example: 12|8@0
        """
        start_length, byte_order = dbc_value.split("@")
        start, length = start_length.split("|")
        return int(start), int(length), int(byte_order)

    def _convert_start_from_dbc_to_index(self, start_in_dbc: int, byte_order: int) -> int:
        if byte_order == 1:  # Intel / Little-Endian: start bit is already the LSB index
            return start_in_dbc
        return start_in_dbc + 7 - 2 * (start_in_dbc % 8)  # Motorola / Big-Endian

    def _normalize_field_name(self, field_name: str) -> str:
        return field_name.strip().lstrip("\ufeff")

    def _normalize_value(self, value: str | None) -> str:
        if value is None:
            return ""
        return value.strip()

    def _parse_int(self, value: str | None, default: int = 0) -> int:
        if not value:
            return default
        return int(float(value))

    def _parse_float(self, value: str | None, default: float = 0.0) -> float:
        if not value:
            return default
        return float(value.replace(",", "."))
