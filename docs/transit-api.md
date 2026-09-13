# Transit API integration notes

Verified from the companion `transit-board` project's source and live HTTPS
requests on 2026-09-13. Controller integration is now installed in 1.5.1;
the transit service itself was not changed.

## Access

- Base URL: `https://transit.tribecans.com/api`
- Authentication: `Authorization: Bearer <token>`.
- The existing ESP32 `.env` setting is **`TRANSIT_API_KEY`**, not `TRANST_API_KEY`.
  Never include the value in source files, logs or examples.
- Live unauthenticated request returned HTTP 401; the same request with the
  configured token returned HTTP 200 and JSON departures.
- The API key is accepted directly by the server. Browser Firebase App Check
  is not required for this device authentication path.
- Firebase Hosting forwards `/api/**` to the `api` function in `europe-west6`.

Verified implementation: `functions/src/index.js`, `src/utils/api.js`,
`functions/src/routes/`, `functions/src/ojp/parsers.js` in the transit project.
Its root README predates the authenticated OJP proxy and is not the current
API contract.

## Read-only endpoints

| Path | Parameters | Result |
| --- | --- | --- |
| `/stationboard` | `stop`, `limit` (default 30) | Departure array |
| `/locations` | `query` | Stop search results |
| `/addresses` | `query` | Address and stop results |
| `/nearby` | `lat`, `lon` | Nearby stops |
| `/reverse-geocode` | `lat`, `lon` | One place or null |
| `/connections` | `from`, `to`, `limit` (default 6) | Journey array |

Example: `GET /api/stationboard?stop=8503009&limit=6` with the bearer header.
All three station IDs below returned HTTP 200 with six departures in live tests.
Six departures occupied about 1.7 KB in the rail-stop response.

Departure fields include `number` (e.g. `S24`, `S8`, `7`, `72`), `to`,
`category`, `departure` (ISO timestamp), `delay` (minutes) and `platform`.
`departure` already uses the estimated time when available: do not add the
delay again when calculating the countdown. Nested `stop.prognosis.departure`
indicates whether an estimated time was supplied. The device needs a correct
clock for countdowns and Europe/Zurich for local clock-time display.

The server caches identical queries for 120 seconds; a shorter polling interval
does not guarantee fresh upstream data. Deduplicate queries for the same stop
and filter the returned rows locally. Handle HTTP 401, 429 and 502 explicitly;
show stale/unavailable status rather than presenting old times as live.

## Candidate boards from local defaults

These are the local `src/config/stations.local.js` defaults, not verified as
the user's current Firestore-saved settings. The API does not expose saved
board preferences through any of the routes above.

| Board | Stop ID | Intended direction from local config |
| --- | --- | --- |
| Wollishofen Bhf/Staubstrasse tram | `8591081` | Stettbach |
| Wollishofen S24 | `8503009` | Weinfelden / Thayngen |
| Wollishofen S8 | `8503009` | Winterthur |
| Jugendherberge bus | `8591216` | Milchbuck |

The user confirmed these four selections. Firmware 1.5.1 queries the three
distinct stops with limit 60, filters exact approved line/destination pairs,
orders by estimated departure timestamp and displays the next three in each
group. Approved alternate spellings above are explicitly recognized. Icons
are assigned by the four known routes, without relying on the category field.

The controller uses ESP-IDF HTTPS with the trusted CA bundle and redirects
disabled. SNTP sets the clock; Europe/Zurich governs displayed local times.
Its API diagnostic confirmed successful direct HTTPS responses for all four
route groups after TLS and general allocations were moved to PSRAM.

## Data differences to resolve

- Live S8, S24 and tram 7 rows all have `category: "B"`. The parser reads
  `service.PtMode` and maps unrecognized modes to B; the exact upstream cause
  has not been verified. Do not silently drop train/tram rows through the old
  category filters. Resolve classification before relying on this field.
- Train numbers now include their prefix (`S8`, `S24`), while the old local
  config filters use `8` and `24`.
- Live destination names include `Bahnhof Stettbach` and `Milchbuck`, while
  old local defaults use `Stettbach, Bahnhof` and `Zürich, Milchbuck`.
- Reusing those old exact-match filters would hide otherwise valid departures.

Live response samples are kept under ignored `logs/transit-*.json`. They contain
public departure data only, no credentials. Local macOS curl successfully
validated TLS; the Python environment's default certificate store could not
validate the presented chain. Certificate verification was never disabled.
