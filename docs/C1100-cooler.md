# Osprey C1100 cooler (HeatSink-C1100)

Osprey Electronics sold an active air cooler for the AMD/Xilinx **Varium C1100**,
which also fits the **Alveo U55C**. It replaces the card's stock passive enclosure.
The product has been **delisted**: its page now returns HTTP 404. This page keeps
what can still be found about it, including the screws needed to mount it, which
Osprey never documented in full.

## At a glance

| | |
|---|---|
| Product | Varium C1100 / Alveo U55C Heat Sink ("Aleo U55C" on the listing) |
| SKU | `HeatSink-C1100` |
| Vendor | Osprey Electronics (Ingenious Technology, LLC) |
| Price | **$100.00**, unchanged in every capture from 2022-10 to 2024-05 |
| Status | "Out of Stock" at the last capture (2024-05-26). The page was deleted after that |
| Board included | No. Cooler only ("FPGA board is not included") |
| Lead time | "Delivery Time: 1 week" |

## Specifications (Osprey's listing)

These are the only specifications Osprey published. The description text is the
same in all five archived captures.

- **Active air cooling** with **2 fans**
- **5 copper heat pipes**
- "**thick layers of copper** for fast heat transmission"
- **PCIe 6-pin power connector**. The fans draw from a 6-pin PCIe lead, not
  from the slot, so a spare 6-pin from the PSU is part of fitting it.
- Fits the **Varium C1100** and the **Alveo U55C**

Osprey published no dimensions, weight, fan size, fan speed, noise level or
thermal rating.

### What the product photos show

These points come from Osprey's photos and renders (links below). They are
observations, not published specs.

- **Two axial fans** under a black shroud with the Osprey logo on each hub.
- **Thicker than the stock enclosure.** The stock C1100 cooler is single-slot.
  This shroud stands proud of the bracket, so leave the neighbouring slot free.
- **Longer than the card.** The shroud and fin stack run past the end of the
  C1100's PCB, which is 167.65 mm long.
- **The card's own backplate stays on.** The rear renders show the stock metal
  backplate and its screws.
- **Copper contact plate** over the FPGA, fed by the heat pipes, carrying a grey
  thermal pad in the photo. The heat pipes loop over the top edge of the card into
  two aluminium fin stacks.
- **The fans are wired to a 4-wire lead**, and the **6-pin PCIe socket** is at
  the end of the shroud.

## Mounting screws

**Osprey's photos give the thread sizes but not the lengths.** The lengths
below were measured on a cooler.

| position | where | qty (per Osprey photo) | screw |
|---|---|---|---|
| **inner** | die clamp: the four holes around the copper contact plate | 4 | **M3 × 10 mm**, spring-loaded |
| **outer** | the four corner brackets on the fin stacks | 4 | **M2.5 × 10 mm** |
| edge | two positions along the edge of the fin stack | 2 | **M2** (length not given) |

- **The cooler is metric throughout.** It is not an imperial `#4-40`/`#6-32`
  part, even though the holes can look that way under a caliper.
- **The inner screws should be the spring-loaded type**, as used on GPU coolers.
  The spring sets the contact pressure on the die. A plain screw of the same
  thread will either under-clamp or crush it.
- **Do not go longer than the lengths above.** The holes are blind. A screw that
  bottoms out feels tight but puts no clamping force on the die. The cooler looks
  properly mounted and makes no thermal contact. On a 75 W card that ends in an
  over-temperature shutdown.
- **The stock heatsink's screws are no guide.** The Osprey cooler's holes are not
  the same size as the stock C1100 enclosure's.

## Before you swap coolers

The C1100 ships single-slot, full-height, half-length and **passively cooled**
at a 75 W limit. It relies on server airflow. AMD's installation guides warn
about removing the stock enclosure:

> If the cooling enclosure is removed from the adaptor and the adaptor is
> powered-up, external fan cooling airflow MUST be applied to prevent
> over-temperature shut-down.

> Removing the cooling enclosure voids the board warranty.

So connect the cooler's 6-pin lead before powering on the card, and never run
the card bare, even briefly.

AMD publishes the card's outline in the C1100 data sheet (DS1003): a 1.57 mm PCB,
167.65 mm long, PCIe CEM rev 3.0 single-slot FHHL. AMD publishes no heatsink
mounting pattern or screw specification.

## Archived product page (Wayback Machine)

The live page (`https://www.ospreyelectronics.io/product-page/c1100-heat-sink`)
is gone. The Internet Archive holds five captures:

| captured | link | notes |
|---|---|---|
| 2022-10-06 | [web.archive.org/web/20221006175106/…](https://web.archive.org/web/20221006175106/https://www.ospreyelectronics.io/product-page/c1100-heat-sink) | earliest capture; six product renders |
| 2023-02-04 | [web.archive.org/web/20230204223511/…](https://web.archive.org/web/20230204223511/https://www.ospreyelectronics.io/product-page/c1100-heat-sink) | |
| 2023-06-02 | [web.archive.org/web/20230602023120/…](https://web.archive.org/web/20230602023120/https://www.ospreyelectronics.io/product-page/c1100-heat-sink) | first capture with the two **screw-location photos** |
| 2023-12-08 | [web.archive.org/web/20231208165223/…](https://web.archive.org/web/20231208165223/https://www.ospreyelectronics.io/product-page/c1100-heat-sink) | |
| 2024-05-26 | [web.archive.org/web/20240526001302/…](https://web.archive.org/web/20240526001302/https://www.ospreyelectronics.io/product-page/c1100-heat-sink) | last capture; **Out of Stock**, $100.00 |

The full list of captures, in case more are added:
[web.archive.org/web/*/ospreyelectronics.io/product-page/c1100-heat-sink*](https://web.archive.org/web/*/ospreyelectronics.io/product-page/c1100-heat-sink*)

### Product photos

The Wayback Machine did not save these images. As of 2026-09-24 Osprey's image
CDN still serves them, but they could disappear at any time.

| photo | link |
|---|---|
| Front, flat: two fans, bracket, PCIe edge | [bdff1d_58ab5e5f…](https://static.wixstatic.com/media/bdff1d_58ab5e5fadd04fe6bc0f2f83b9d83cce~mv2.jpg) |
| Front, three-quarter view | [bdff1d_1aaa3459…](https://static.wixstatic.com/media/bdff1d_1aaa3459a9a5423690b9f4bfd36bfcb3~mv2.jpg) |
| Bracket end, showing shroud thickness | [bdff1d_42224fa4…](https://static.wixstatic.com/media/bdff1d_42224fa4bd164b87b7f09e5814b40455~mv2.jpg) |
| Rear, three-quarter: backplate and heat pipes | [bdff1d_f6289ad9…](https://static.wixstatic.com/media/bdff1d_f6289ad98b5c4146abd1fe6da991f7a2~mv2.jpg) |
| Rear, flat: backplate, heat pipes over the top edge | [bdff1d_3d57e84d…](https://static.wixstatic.com/media/bdff1d_3d57e84d98784c8886ebb3223695a692~mv2.jpg) |
| Rear, from the far end: fin stack and 6-pin socket | [bdff1d_93a48d26…](https://static.wixstatic.com/media/bdff1d_93a48d2603634c1285fdc324cbf31460~mv2.jpg) |
| **Screw locations**: underside with contact plate, M3 (inner) and M2.5 (outer) labelled | [b9768a_0d9ee8d3…](https://static.wixstatic.com/media/b9768a_0d9ee8d3097c41198ca841d3abd16814~mv2.jpg) |
| **Screw locations**: fin-stack edge, two M2 positions labelled | [b9768a_d2a17cd2…](https://static.wixstatic.com/media/b9768a_d2a17cd2789341178e902abad2cba595~mv2.jpg) |

## Listing text, verbatim (2024-05-26 capture)

> Varium C1100 / Aleo U55C Heat Sink SKU: HeatSink-C1100 $100.00 Price Quantity
> Out of Stock Notify When Available DESCRIPTION PAYMENT AND NOTE FPGA board is
> not included. Heatsink for Xilinx Varium C1100\* / Aleo U55C Special designed
> active air cooling, including 2 fans, 5 copper heat pipes, thick layers of
> copper for fast heat transmission. PCIe 6 Pin power connector. \* Delivery
> Time: 1 week \*Xilinx Varium C1100: is a Xilinx product designed to accelerate
> compute-intensive blockchain applications, including transaction validation
> and hash calculations used in most Proof of Work (PoW) and Proof of Stake (PoS)
> algorithms.
