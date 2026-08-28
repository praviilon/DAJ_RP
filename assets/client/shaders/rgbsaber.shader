// GalaxyRP: [Saber RGB] tintable saber blade shaders.
//
// The base game's six blade shaders (gfx/effects/sabers/red_line, blue_glow, ...) each bake their
// colour into the texture, which is why a saber can only ever be one of six colours. These two are
// the same artwork drawn in neutral greyscale (see scripts/gen_rgb_saber_textures.py, which
// generates both maps) with "rgbGen vertex" instead: that tells the renderer to multiply the
// texture by the per-entity colour the surface was submitted with, so one shader can draw any
// colour. CG_DoSaber() and UI_DoSaber() select these whenever a blade's colour is SABER_RGB and
// set refEntity_t::shaderRGBA to the player's own packed colour.
//
// Everything else here matches the base blade shaders exactly -- same additive blend, same
// twosided cull, same nopicmip -- so a custom-coloured blade sits in the scene identically to a
// palette-coloured one, and "glow" keeps it picked up by the bloom pass like the originals.

gfx/effects/sabers/rgb_glow
{
	nopicmip
	notc
	cull twosided
	{
		map gfx/effects/sabers/rgb_glow
		blendFunc GL_ONE GL_ONE
		glow
		rgbGen vertex
	}
}

gfx/effects/sabers/rgb_line
{
	nopicmip
	notc
	cull twosided
	{
		map gfx/effects/sabers/rgb_line
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
}
