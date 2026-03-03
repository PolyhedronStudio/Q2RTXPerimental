# IMPORTANT
The shader in the ``/shader/`` folder may be different than required for your TrenchBroom folder.
Solutions are either:
- I supply for the time being, custom TB binaries with each release.
- Cooperate with TB developers, in order to add configuration for how to treat alpha in textures.
Q2RTX and Q2RTXP use a mask texture for alpha instead. Similar behavior is favored then, in TB.

# CURRENT SOLUTION:
Scroll to about line 51, where it says ``    // Assume alpha masked or opaque.``
Replace the GLSL code block below with the following one instead:
```    
    // TODO: Make this optional if we gain support for translucent textures
    //if (EnableMasked && gl_FragColor.a < 0.5) {
    // WID: Temporary fix for Q2RTXP which has alpha channel usage as roughness storage PBR.
    if (ApplyMaterial && EnableMasked && gl_FragColor.a < 0.01) {
        discard;
    }
```