#include <paint/floating.h>

#include <paint/stroke.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace olagarro::paint
{

namespace
{

// Whether a transform keeps pixels whole: moved by whole pixels, mirrored or turned by quarter turns, not scaled.
[[nodiscard]] bool exact(const PieceTransform& transform)
{
    constexpr auto quarter{3.14159265358979323846 / 2.0};
    const auto turns{transform.radians / quarter};
    return std::abs(turns - std::round(turns)) < 1e-9 && std::abs(std::abs(transform.scale_x) - 1.0) < 1e-12 &&
           std::abs(std::abs(transform.scale_y) - 1.0) < 1e-12 &&
           std::abs(transform.dx - std::round(transform.dx)) < 1e-9 &&
           std::abs(transform.dy - std::round(transform.dy)) < 1e-9;
}

} // namespace

FloatingPiece lift_piece(const Canvas& canvas, const Selection& selection)
{
    const auto lifted{lift_selected(canvas, selection)};
    FloatingPiece piece{lifted.pixels, Image{PixelFormat::gray_alpha8, lifted.pixels.width, lifted.pixels.height},
                        lifted.x, lifted.y};
    for(int y{}; y < piece.mask.height; ++ y)
    {
        for(int x{}; x < piece.mask.width; ++ x)
        {
            piece.mask.set_pixel(x, y, GrayAlpha8{{selection.at(piece.x + x, piece.y + y), 255}});
        }
    }
    return piece;
}

Affine piece_forward(const FloatingPiece& piece, const PieceTransform& transform)
{
    const auto center_x{piece.x + piece.pixels.width / 2.0};
    const auto center_y{piece.y + piece.pixels.height / 2.0};
    return Affine::translation(center_x + transform.dx, center_y + transform.dy)
        .after(Affine::rotation(transform.radians))
        .after(Affine::scale(transform.scale_x, transform.scale_y))
        .after(Affine::translation(-center_x, -center_y));
}

PieceTransform turned_round(const FloatingPiece& piece, PieceTransform start, Position pivot, double radians)
{
    const auto middle_x{piece.x + piece.pixels.width / 2.0 + start.dx - pivot.x};
    const auto middle_y{piece.y + piece.pixels.height / 2.0 + start.dy - pivot.y};
    auto turned{start};
    turned.radians += radians;
    turned.dx += middle_x * std::cos(radians) - middle_y * std::sin(radians) - middle_x;
    turned.dy += middle_x * std::sin(radians) + middle_y * std::cos(radians) - middle_y;
    return turned;
}

std::array<Position, 4> piece_corners(const FloatingPiece& piece, const PieceTransform& transform)
{
    const auto forward{piece_forward(piece, transform)};
    const double left{static_cast<double>(piece.x)};
    const double top{static_cast<double>(piece.y)};
    const auto right{left + piece.pixels.width};
    const auto bottom{top + piece.pixels.height};
    std::array<Position, 4> corners{Position{left, top}, Position{right, top}, Position{right, bottom},
                                    Position{left, bottom}};
    for(auto& corner : corners)
    {
        forward.apply(corner.x, corner.y, corner.x, corner.y);
    }
    return corners;
}

void place_piece(Canvas& canvas, const Canvas& base, Selection& selection, const FloatingPiece& piece,
                 const PieceTransform& transform, bool exact_pixels)
{
    canvas = base;
    const auto corners{piece_corners(piece, transform)};
    auto left{1e18};
    auto top{1e18};
    auto right{-1e18};
    auto bottom{-1e18};
    for(const auto& corner : corners)
    {
        left = std::min(left, corner.x);
        top = std::min(top, corner.y);
        right = std::max(right, corner.x);
        bottom = std::max(bottom, corner.y);
    }
    // Rounded out, past half a pixel of error, so an exact transform lands on whole pixels.
    const auto box_x{static_cast<int>(std::floor(left + 1e-6))};
    const auto box_y{static_cast<int>(std::floor(top + 1e-6))};
    const auto box_width{std::max(1, static_cast<int>(std::ceil(right - 1e-6)) - box_x)};
    const auto box_height{std::max(1, static_cast<int>(std::ceil(bottom - 1e-6)) - box_y)};
    // From a pixel of the box to the piece's own pixels.
    const auto to_piece{Affine::translation(-piece.x, -piece.y)
                            .after(piece_forward(piece, transform).inverse())
                            .after(Affine::translation(box_x, box_y))};
    const auto resampling{exact_pixels || exact(transform) ? Resampling::nearest : Resampling::bilinear};
    const auto pixels{transformed(piece.pixels, to_piece, box_width, box_height, resampling)};
    static_cast<void>(lay_over(canvas, pixels, box_x, box_y));
    const auto mask{transformed(piece.mask, to_piece, box_width, box_height, resampling)};
    Canvas moved_mask{PixelFormat::gray_alpha8, selection.width(), selection.height()};
    Image selected{PixelFormat::gray_alpha8, box_width, box_height};
    for(int y{}; y < box_height; ++ y)
    {
        for(int x{}; x < box_width; ++ x)
        {
            const auto pixel{mask.pixel<GrayAlpha8>(x, y)};
            // Outside the piece the mask is see-through, which is not selected.
            selected.set_pixel(x, y,
                               GrayAlpha8{{static_cast<std::uint8_t>(pixel.value[0] * pixel.value[1] / 255), 255}});
        }
    }
    // The selection stays inside the image.
    const auto clip_left{std::max(0, box_x)};
    const auto clip_top{std::max(0, box_y)};
    const auto clip_right{std::min(selection.width(), box_x + box_width)};
    const auto clip_bottom{std::min(selection.height(), box_y + box_height)};
    if(clip_left < clip_right && clip_top < clip_bottom)
    {
        Image inside{PixelFormat::gray_alpha8, clip_right - clip_left, clip_bottom - clip_top};
        for(auto y{clip_top}; y < clip_bottom; ++ y)
        {
            for(auto x{clip_left}; x < clip_right; ++ x)
            {
                inside.set_pixel(x - clip_left, y - clip_top, selected.pixel<GrayAlpha8>(x - box_x, y - box_y));
            }
        }
        moved_mask.write(inside, clip_left, clip_top);
    }
    selection.set_mask(std::move(moved_mask));
}

} // namespace olagarro::paint
