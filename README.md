# Streetview Client
A reverse engineered Google Maps streetview client in C++ that lets you download panoramas and render them.

# Usage
```
Download panoramas
Usage: ./streetview_client download [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  --lat FLOAT REQUIRED        Latitude
  --long FLOAT REQUIRED       Longitude
  -r,--range INT              Range from location
  -n,--num-panoramas INT      Number of panoramas to attempt to download
  -z,--zoom INT               How much to zoom in street view images, higher numbers increase resolution

Render panoramas
Usage: ./streetview_client render [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  -i,--id TEXT REQUIRED       Initial panorama ID
```