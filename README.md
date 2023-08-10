# Wavemap

![wavemap_cover](https://github.com/ethz-asl/wavemap/assets/6238939/0df66963-3871-4fae-8567-523518c43494)

## Hierarchical, multi-resolution volumetric mapping

Wavemap achieves state-of-the-art memory and computational efficiency by combining Haar wavelet compression and a coarse to fine measurement integration scheme. The use of advanced measurement models allow it to attain exceptionally high recall rates on challenging obstacles such as thin objects.

## Documentation
The framework's documentation is hosted on [GitHub Pages](https://ethz-asl.github.io/wavemap/).

### Table of contents
* [Installation](https://ethz-asl.github.io/wavemap/pages/installation.html)
* [Demos](https://ethz-asl.github.io/wavemap/pages/demos.html)
* [Configuration](https://ethz-asl.github.io/wavemap/pages/configuration.html)
* [Usage examples](https://ethz-asl.github.io/wavemap/pages/usage_examples.html)
* [Contributing](https://ethz-asl.github.io/wavemap/pages/contributing.html)
* [Library API](https://ethz-asl.github.io/wavemap/api/library_root.html)
* [FAQ](https://ethz-asl.github.io/wavemap/pages/faq.html)

## Paper
A technical introduction to the theory behind the wavemap is provided in our open-access [RSS paper](https://www.roboticsproceedings.org/rss19/p065.pdf).

Please cite it when using wavemap for research:

Reijgwart, V., Cadena, C., Siegwart, R., & Ott, L. (2023). Efficient volumetric mapping of multi-scale environments using wavelet-based compression. Proceedings of Robotics: Science and System XIX.

```
@INPROCEEDINGS{reijgwart2023wavemap,
    author = {Reijgwart, Victor and Cadena, Cesar and Siegwart, Roland and Ott, Lionel},
    journal = {Robotics: Science and Systems. Online Proceedings},
    title = {Efficient volumetric mapping of multi-scale environments using wavelet-based compression},
    year = {2023-07},
}
```

<details>
<summary>Abstract</summary>
<br>
Volumetric maps are widely used in robotics due to their desirable properties in applications such as path planning, exploration, and manipulation. Constant advances in mapping technologies are needed to keep up with the improvements in sensor technology, generating increasingly vast amounts of precise measurements. Handling this data in a computationally and memory-efficient manner is paramount to representing the environment at the desired scales and resolutions. In this work, we express the desirable properties of a volumetric mapping framework through the lens of multi-resolution analysis. This shows that wavelets are a natural foundation for hierarchical and multi-resolution volumetric mapping. Based on this insight we design an efficient mapping system that uses wavelet decomposition. The efficiency of the system enables the use of uncertainty-aware sensor models, improving the quality of the maps. Experiments on both synthetic and real-world data provide mapping accuracy and runtime performance comparisons with state-of-the-art methods on both RGB-D and 3D LiDAR data. The framework is open-sourced to allow the robotics community at large to explore this approach.
</details>

Note that the code has significantly improved since the paper was written. Wavemap is now up to 10x faster, thanks to new multi-threaded measurement integrators, and uses up to 50% less RAM, by virtue of new memory efficient data structures inspired by [OpenVDB](https://github.com/AcademySoftwareFoundation/openvdb).
