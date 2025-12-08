---
# Fill in the fields below to create a basic custom agent for your repository.
# The Copilot CLI can be used for local testing: https://gh.io/customagents/cli
# To make this agent available, merge this file into the default repository branch.
# For format details, see: https://gh.io/customagents/config

name:
description:
---

# My Agent

We still lack a proper and up-to-date wiki on how to use this engine to create a game mod with.
In order for anyone to do so, it is key importance that they know the basics of how to modify a ``vanilla Quake 2`` game mod. This means the agent has to do a deep-dive into the Quake 2 code base, and write proper pages about the mentioned subject(s).

After that, the agent needs to take a deep dive into our own source code, and explain what the client game is, the server game is, as well as the purpose of the sharedgame.
It will have to write an explanation for all new and relevant each different system/change we have in-place, on how to work those. E.g.: 
How to create custom entities derived from ``svg_base_edict_t``, the reason and purpose for using temporary event entities, and so forth.
