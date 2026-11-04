#pragma once

#include <paint/canvas.h>
#include <paint/geometry.h>
#include <paint/selection.h>
#include <paint/transform.h>

#include <array>
#include <vector>

namespace olagarro::paint
{

// Pixels lifted off a canvas to be moved, turned and scaled before they are put down: what they are, each as
// see-through as the selection left it, how much each was selected, and where they were.
struct FloatingPiece
{
    Image pixels;
    // Grey: how much each pixel was selected; solid.
    Image mask;
    int x{0};
    int y{0};
};

// Where a piece goes from where it was: moved, then turned and scaled round its middle. A negative scale mirrors it.
struct PieceTransform
{
    double dx{0.0};
    double dy{0.0};
    double radians{0.0};
    double scale_x{1.0};
    double scale_y{1.0};
};

// Lifts what `selection` selects of `canvas`; all of the canvas when nothing is selected.
[[nodiscard]] FloatingPiece lift_piece(const Canvas& canvas, const Selection& selection);
// From the piece's pixels where it was, to where they go.
[[nodiscard]] Affine piece_forward(const FloatingPiece& piece, const PieceTransform& transform);
// The piece's corners where they go: top left, top right, bottom right, bottom left.
[[nodiscard]] std::array<Position, 4> piece_corners(const FloatingPiece& piece, const PieceTransform& transform);
// `start` turned `radians` further round `pivot`, in the image's pixels: the piece's middle goes round it as the rest
// does.
[[nodiscard]] PieceTransform turned_round(const FloatingPiece& piece, PieceTransform start, Position pivot,
                                          double radians);
// Lays the piece, transformed, over `base` into `canvas`, and makes the selection its mask where it went. A transform
// that only moves, mirrors or turns by quarter turns keeps every pixel exactly. Returns nothing; the caller redraws.
// With `exact`, every transform takes the nearest pixel, as indices need.
void place_piece(Canvas& canvas, const Canvas& base, Selection& selection, const FloatingPiece& piece,
                 const PieceTransform& transform, bool exact = false);

} // namespace olagarro::paint
