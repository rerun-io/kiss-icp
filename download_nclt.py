#!/usr/bin/env python3
from __future__ import annotations

import pathlib
from pathlib import Path
import requests
import shutil
import tqdm

SESSION = "2013-01-10" # This session was the shortest.
NLCT_VELODYNE_URL = f"https://s3.us-east-2.amazonaws.com/nclt.perl.engin.umich.edu/velodyne_data/{SESSION}_vel.tar.gz"
NLCT_HOKUYO_URL = f"https://s3.us-east-2.amazonaws.com/nclt.perl.engin.umich.edu/hokuyo_data/{SESSION}_hokuyo.tar.gz"
NLCT_GROUND_TRUTH_URL = f"https://s3.us-east-2.amazonaws.com/nclt.perl.engin.umich.edu/ground_truth/groundtruth_{SESSION}.csv"
data_dir = Path("data")
session_dir = data_dir / Path(SESSION)

def download_file(url: str, dst_file_path: pathlib.Path) -> None:
    """Download file from url to dst_fpath."""
    dst_file_path.parent.mkdir(parents=True, exist_ok=True)
    print(f"Downloading {url} to {dst_file_path}")
    response = requests.get(url, stream=True)
    with tqdm.tqdm.wrapattr(
        open(dst_file_path, "wb"),
        "write",
        miniters=1,
        total=int(response.headers.get("content-length", 0)),
        desc=f"Downloading {dst_file_path.name}",
    ) as f:
        for chunk in response.iter_content(chunk_size=4096):
            f.write(chunk)

def unar_file(file_path: Path):
    shutil.unpack_archive(file_path, extract_dir=file_path.parent, filter="data")

def download_all() -> None:
    lidar_path = data_dir / Path("velodyne.tar.gz")
    if not lidar_path.exists():
        download_file(NLCT_VELODYNE_URL, lidar_path)
        unar_file(lidar_path)
    
    csv_path = data_dir / "ground_truth" / Path(f"groundtruth_{SESSION}.csv")
    if not csv_path.exists():
        download_file(NLCT_GROUND_TRUTH_URL, csv_path)

def main():
    download_all()
    
if __name__ == "__main__":
    main()

