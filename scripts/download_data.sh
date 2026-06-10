#!/usr/bin/env bash
# Downloads one day of Binance spot trades into data/.
# Usage: scripts/download_data.sh BTCUSDT 2024-01-15
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "Usage: $0 <SYMBOL> <YYYY-MM-DD>" >&2
    echo "Example: $0 BTCUSDT 2024-01-15" >&2
    exit 1
fi

symbol="$1"
date="$2"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
data_dir="${script_dir}/../data"
zip_name="${symbol}-trades-${date}.zip"
url="https://data.binance.vision/data/spot/daily/trades/${symbol}/${zip_name}"

mkdir -p "${data_dir}"
echo "Downloading ${url}"
if ! curl -f -o "${data_dir}/${zip_name}" "${url}"; then
    echo "Error: download failed. Check the symbol and date (the dump for a" >&2
    echo "recent date may not be published yet on data.binance.vision)." >&2
    exit 1
fi

unzip -o "${data_dir}/${zip_name}" -d "${data_dir}"
rm "${data_dir}/${zip_name}"
echo "Done: ${data_dir}/${symbol}-trades-${date}.csv"
