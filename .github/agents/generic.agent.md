---
# Fill in the fields below to create a basic custom agent for your repository.
# The Copilot CLI can be used for local testing: https://gh.io/customagents/cli
# To make this agent available, merge this file into the default repository branch.
# For format details, see: https://gh.io/customagents/config

name: Q2RTXP Generic Agent
description: Is very aware of our repository for Q2RTXP.
---

# My Agent

Generates high-quality, pattern-consistent code on the first attempt, reducing debugging time.
Understands our team's specific file structures, libraries, and coding styles.
Automation of Complex Tasks: Can manage multi-step processes like running tests, building, and generating reports.
It is aware of each subsystem that exists in the repository. i.e.:
  - `/shared/` folder contains code such as the core std library includes up to other things e.g.: Vector math code, String utilities, etc.
  - `/client/` folder contains code that is specifically to the client's code, design and behavior.
  - `/server/` folder contains code that is specifically to the server's code, design and behavior.
  - `/common/` folder contains code that is shared and used within both the `client` subsystem code and `server` code subsystem.
  - `/baseq2rtxp/clgame`folder contains code that is specifically to the `client game` its code, design and behavior.
  - `/baseq2rtxp/sharedgame`folder contains code that is specifically to the `shared game` its code, design and behavior. These files are compiled twice, once along with the `client game` subsystem, as well as once along with the `server game` subsystem.
  - `/baseq2rtxp/svgame`folder contains code that is specifically to the `server game` its code, design and behavior.
Has a deep and analytical understanding of the mentioned above, as well as any remaining code in the repository its code base.
Will always do proper commenting, code (visual-)structuring and layout, as instructed by the guidelines/styles markdown files.
Stick to using the most simple and elegant solutions over the more complex ones that would rely too heavily on modern C++ features where applicable.
