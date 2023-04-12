# Streetview Client
A reverse engineered Google Maps streetview client in C++ that lets you download panoramas and render them.

![Stationary viewer](./images/example.gif "Stationary viewer")
![Navigate with arrow keys](./images/example2.gif "Navigate with arrow keys")

# Usage
```
Download panoramas
Usage: ./streetview_client download [OPTIONS] [SUBCOMMAND]

Options:
  -h,--help                   Print this help message and exit
  --lat FLOAT REQUIRED        Latitude
  --long FLOAT REQUIRED       Longitude
  -r,--range INT              Range from location, unit not known
  --month-start INT           Starting month
  --month-end INT             Ending month (inclusive)
  --year-start INT            Starting year
  --year-end INT              Ending year (inclusive)
  --path-format TEXT          Path format, not including the extension. Supports {id}, {year}, {month}, {lat}, {long}, {street}, {city}
  -n,--num-panoramas INT      Number of panoramas to attempt to download
  -z,--zoom INT               Dimensions of street view images, higher numbers increase resolution. Usually 1=832x416, 2=1664x832, 3=3328x1664, 4=6656x3328, 5=13312x6656 (glitched at the poles)
  -j,--json                   Include JSON info alongside panorama

Subcommands:
  recursive                   Recursively attempt to download nearby panoramas
```

```
Recursively attempt to download nearby panoramas
Usage: ./streetview_client download recursive [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  -a,--num-attempts INT       Number of recursive attempts to download more images
  -r,--radius FLOAT           Radius of images to download in latitude degrees
```

```
Render panoramas in viewer
Usage: ./streetview_client render [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  -i,--id TEXT REQUIRED       Initial panorama ID
  -z,--zoom INT               Dimensions of street view images, same as download -z
  --month-start INT           Starting month
  --month-end INT             Ending month (inclusive)
  --year-start INT            Starting year
  --year-end INT              Ending year (inclusive)
```

# Client
`./streetview_client render` is a simplified Streetview client that allows you to look around and navigate to adjacent panoramas. Look around with drag, zoom in with scroll and move to adjacent panoramas with the up arrow. `./streetview_client download` is a quick downloader that directly downloads panoramas around a location. `./streetview_client download recursive` is a quick downloader that repeatedly requests panoramas close to a location in order to download every single panorama in a radius.

# Example commands
```
./streetview_client download --lat 42.360017 --long -71.058284 --path-format panoramas_boston/{id}-{street}-{year}-{month} -z 2 -n 1000
```
This command will attempt to download 1000 panoramas around Boston with their id, street, year and month in the filename with dimensions of 1664x832.
```
./streetview_client download --lat 52.08855495179819 --long 5.124632840963613 --path-format panoramas_utrecht/{id} -z 4 recursive -a 100 -r 0.00005
```
This command will attempt to recursively download nearby panoramas around Utrecht, Netherlands 100 times in a radius of approximately 18.2 feet (there are approximately 364,000 feet in a latitude degree and 0.00005 * 364,000 = 18.2) with dimensions of 6656x3328.
```
./streetview_client render -z 2 -i 7RP3sV6czwHDli2hSTkB8A
```
This command will allow you to walk around in Boston with panoramas of dimension 1664x832.