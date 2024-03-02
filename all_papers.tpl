
<!DOCTYPE html>
<html lang="en-US">
  <head>
    <meta charset="UTF-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1">

<title>Papers with Numbers | cpp_proposals</title>
<meta property="og:title" content="Papers with Numbers" />
<meta property="og:locale" content="en_US" />
<meta name="description" content="My WG21 proposals" />
<meta property="og:description" content="My WG21 proposals" />
<link rel="canonical" href="https://brevzin.github.io/cpp_proposals/all_papers.html" />
<meta property="og:url" content="https://brevzin.github.io/cpp_proposals/all_papers.html" />
<meta property="og:site_name" content="cpp_proposals" />
<meta property="og:type" content="website" />
<meta name="twitter:card" content="summary" />
<meta property="twitter:title" content="Papers with Numbers" />
<script type="application/ld+json">
{"@context":"https://schema.org","@type":"WebPage","description":"My WG21 proposals","headline":"Papers with Numbers","url":"https://brevzin.github.io/cpp_proposals/all_papers.html"}</script>

    <link rel="stylesheet" href="/cpp_proposals/assets/css/style.css?v=af5093e70704604705d01ddad6d7eea8029c8ca9">
    <!-- start custom head snippets, customize with your own _includes/head-custom.html file -->

<!-- Setup Google Analytics -->



<!-- You can set your favicon here -->
<!-- link rel="shortcut icon" type="image/x-icon" href="/cpp_proposals/favicon.ico" -->

<!-- end custom head snippets -->

  </head>
  <body>
    <div class="container-lg px-3 my-5 markdown-body">

      <h1><a href="https://brevzin.github.io/cpp_proposals/">cpp_proposals</a></h1>


      <h1 id="papers-with-numbers">Papers with Numbers</h1>
<ul>
  {% for paper in numbered_papers %}
    <li>{% if paper.badge %}<img src="https://img.shields.io/badge/{{paper.badge}}" alt="" />{% endif %}{{paper.number}} {{paper.title}}: {% for rev in paper.revisions %}<a href="{{rev.href}}">{{rev.name}}</a> {% endfor %}</li>
  {% endfor %}
</ul>

<h1 id="other-papers">Other Papers</h1>
<ul>
  <li>Concepts, v2: <a href="/cpp_proposals/concepts_v2/concepts_v2.html">concepts_v2.html</a></li>
  <li>Exploring Placeholders: <a href="/cpp_proposals/xxxx_placeholders/dxxxxr0.html">dxxxxr0.html</a></li>
  <li>Various Designs for Pipelines: <a href="/cpp_proposals/xxxx_placeholders/pipelines.html">pipelines.html</a></li>
  <li>Exploring Placeholders: <a href="/cpp_proposals/xxxx_placeholders/placeholders.html">placeholders.html</a></li>
  <li>Variadic Friends: <a href="variadic_friends/variadic_friends.html">variadic_friends.html</a></li>
</ul>



    </div>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/anchor-js/4.1.0/anchor.min.js" integrity="sha256-lZaRhKri35AyJSypXXs4o6OPFTbTmUoltBbDCbdzegg=" crossorigin="anonymous"></script>
    <script>anchors.add();</script>
  </body>
</html>
