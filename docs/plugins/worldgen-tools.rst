worldgen-tools
========

.. dfhack-tool::
    :summary: Useful tools for controlling the process of world generation.
    :tags: unavailable

.. dfhack-command:: worldgen-autopause
   :summary: Configure automatic pausing of world generation.

.. dfhack-command:: worldgen-reject
   :summary: Reject the current world.
   
.. dfhack-command:: worldgen-pause
   :summary: Pause current world generation

Automatic Pausing
-----------------

Automatic pausing of world generation can be configured for any of the following
stages:

 * `PreparingElevation`
 * `SettingTemperature`
 * `RunningRivers`
 * `FormingLakesAndMinerals`
 * `GrowingVegetation`
 * `VerifyingTerrain`
 * `ImportingWildlife`
 * `RecountingLegends`
 * `PlacingCaves`
 * `PlacingGoodEvil`
 * `PlacingMegabeasts`
 * `PlacingOtherBeasts`
 * `PlacingCavePops`
 * `PlacingCaveCivs`
 * `PlacingCivs`

Additionally, `allstages` configures pausing at all the stages.

A list of years may also be specified, or `yearly` to pause at every year, if possible.

Finally, `all` will configure automatic pauses at every possible opportunity and
`clear` will remove all configured pauses.

.. warning::
        
    Given that several of the world generation stages happen very quickly
    and/or at the same time, it can happen that stages that would cause
    an automatic pause are already finished before DFHack can react to that.
    In such cases, the automatic pause will happen at the next update, which
    may correspond to a different stage.
    The same applies to years. 


Worldgen Rejection
------------------

Worlds are rejected by increasing the number of civilizations to add to a value
that simply cannot be made to fit even in the largest size. This, however, means
that world generation will continue to happen up to the stage where civilizations
are placed.

It is not possible to reject a world after all civilizations have been placed.

Usage
-----

``worldgen-autopause``
    Prints the current automatic pause configuration.
    
``worldgen-autopause <args> ...``
    Configures automatic pausing according to the arguments.
    
``worldgen-reject``
    Reject the current world.

``worldgen-pause [<value>]``
    Pause current world generation. If the additional argument
    is provided, `1` and `true` pause while `0` and `false` unpause.
    
``worldgen-tools autopause [<args> ...]``
    An alias of `worldgen-autopause [<args> ...]`.

``worldgen-tools reject``
    An alias of `worldgen-reject`.
    
``worldgen-tools pause``
    An alias of `worldgen-pause 1`.
    
``worldgen-tools unpause``
    An alias of `worldgen-pause 0`.
    
Examples
--------

``worldgen-autopause MakingCivs``
    Pause world generation while placing civilizations.
    **This is the last chance to reject a world.**

``worldgen-autopause clear``
    Clear the current automatic pause configuration.
    
``worldgen-autopause all``
    Pause at every possible moment during worldgen.

``worldgen-autopause allstages``
    Pause at every world generation stage before generating history.
  
``worldgen-autopause yearly``
    Pause every year (or every update tick) while generating history.

``worldgen-autopause 1 7 42``
    Pause after the years 1, 7 and 42 have been generated.
    
``worldgen-autopause RunningRivers PlacingCaves 5 53``
    A combination of automatic pause configurations
