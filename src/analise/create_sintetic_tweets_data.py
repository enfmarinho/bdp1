import pandas as pd
import numpy as np

rng = np.random.default_rng(42)
n_per_class = 1000

regions = {
    "Northeast": {"lon": -73.8, "lat": 42.2, "lon_sd": 0.7, "lat_sd": 0.5, "timezone": 1},
    "Southeast": {"lon": -80.8, "lat": 33.8, "lon_sd": 0.8, "lat_sd": 0.6, "timezone": 1},
    "Central":   {"lon": -96.8, "lat": 38.5, "lon_sd": 1.0, "lat_sd": 0.8, "timezone": 2},
    "Mountain":  {"lon": -110.0, "lat": 39.0, "lon_sd": 1.0, "lat_sd": 0.8, "timezone": 3},
    "Pacific":   {"lon": -122.3, "lat": 47.0, "lon_sd": 0.8, "lat_sd": 0.6, "timezone": 4},
}

rows = []
start = pd.Timestamp("2013-01-01 00:00:00")
end = pd.Timestamp("2013-01-31 23:59:59")
span_seconds = int((end - start).total_seconds())

for class_name, region in regions.items():
    offsets = rng.integers(0, span_seconds + 1, size=n_per_class)
    timestamps = start + pd.to_timedelta(offsets, unit="s")
    longitudes = rng.normal(region["lon"], region["lon_sd"], size=n_per_class)
    latitudes = rng.normal(region["lat"], region["lat_sd"], size=n_per_class)

    for lon, lat, ts in zip(longitudes, latitudes, timestamps):
        rows.append({
            "Longitude": round(float(lon), 6),
            "Latitude": round(float(lat), 6),
            "Timestamp": int(ts.strftime("%Y%m%d%H%M%S")),
            "Timezone": region["timezone"],
            "Class": class_name,
        })

df = pd.DataFrame(rows).sample(frac=1, random_state=42).reset_index(drop=True)
out_path = "test_tweets_5000.csv"
df.to_csv(out_path, index=False)

print(f"Created: {out_path}")
print(f"Rows: {len(df)}")
print("Rows per class:")
print(df["Class"].value_counts().sort_index().to_string())
print("Columns:", ", ".join(df.columns))
