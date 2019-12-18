const parseDomain = require('parse-domain')

const queriedIds = new Set()
const queriedClasses = new Set()
const regexWhitespace = /\s/

const getClassesAndIds = function (addedNodes: Element[]) {
  const ids = []
  const classes = []

  for (const node of addedNodes) {
    let nodeId = node.id
    if (nodeId && nodeId.length !== 0) {
      nodeId = nodeId.trim()
      if (!queriedIds.has(nodeId) && nodeId.length !== 0) {
        ids.push(nodeId)
        queriedIds.add(nodeId)
      }
    }
    let nodeClass = node.className
    if (nodeClass && nodeClass.length !== 0 && !regexWhitespace.test(nodeClass)) {
      if (!queriedClasses.has(nodeClass)) {
        classes.push(nodeClass)
        queriedClasses.add(nodeClass)
      }
    } else {
      let nodeClasses = node.classList
      if (nodeClasses) {
        let j = nodeClasses.length
        while (j--) {
          const nodeClassJ = nodeClasses[j]
          if (queriedClasses.has(nodeClassJ) === false) {
            classes.push(nodeClassJ)
            queriedClasses.add(nodeClassJ)
          }
        }
      }
    }
  }
  return { classes, ids }
}

const handleNewNodes = (newNodes: Element[]) => {
  const { classes, ids } = getClassesAndIds(newNodes)
  chrome.runtime.sendMessage({
    type: 'classIdStylesheet',
    classes,
    ids
  })
}

const applyCosmeticFilterMutationObserver = () => {
  let targetNode = document.documentElement
  let observer = new MutationObserver(mutations => {
    const nodeList: Element[] = []
    for (const mutation of mutations) {
      for (let nodeIndex = 0; nodeIndex < mutation.addedNodes.length; nodeIndex++) {
        nodeList.push(mutation.addedNodes[nodeIndex] as Element)
      }
    }
    handleNewNodes(nodeList)
  })
  let observerConfig = {
    childList: true,
    subtree: true
  }
  observer.observe(targetNode, observerConfig)
}

const _parseDomainCache = Object.create(null)
const getParsedDomain = (aDomain: any) => {
  const cacheResult = _parseDomainCache[aDomain]
  if (cacheResult !== undefined) {
    return cacheResult
  }

  const newResult = parseDomain(aDomain)
  _parseDomainCache[aDomain] = newResult
  return newResult
}

const _parsedCurrentDomain = getParsedDomain(window.location.host)
const isFirstPartyUrl = (url: string): boolean => {
  if (url.startsWith('/')) {
    return true
  }

  const parsedTargetDomain = getParsedDomain(url)
  if (!parsedTargetDomain) {
    // If we cannot determine the partiness of the resource,
    // consider it first-party.
    console.debug(`Unable to determine party-ness of "${url}"`)
    return false
  }

  return (
    _parsedCurrentDomain.tld === parsedTargetDomain.tld &&
    _parsedCurrentDomain.domain === parsedTargetDomain.domain
  )
}

interface IsFirstPartyQueryResult {
  foundFirstPartyResource: boolean,
  foundThirdPartyResource: boolean,
  foundKnownThirdPartyAd: boolean
}

/**
 * Determine whether a given subtree should be considered as "first party" content.
 *
 * Uses the following process in making this determination.
 *   - If the subtree contains any first party resources, the subtree is first party.
 *   - If the subtree contains no remote resources, the subtree is first party.
 *   - Otherwise, its 3rd party.
 *
 * Note that any instances of "url(" or escape charaters in style attributes are
 * automatically treated as third-party URLs.  These patterns and special cases
 * were generated from looking at patterns in ads with resources in the style
 * attribute.
 *
 * Similarly, an empty srcdoc attribute is also considered third party, since many
 * third party ads clear this attribute in practice.
 *
 * Finally, special case some ids we know are used only for third party ads.
 */
const isSubTreeFirstParty = (elm: Element, possibleQueryResult?: IsFirstPartyQueryResult): boolean => {
  let queryResult: IsFirstPartyQueryResult
  let isTopLevel: boolean

  if (possibleQueryResult) {
    queryResult = possibleQueryResult as IsFirstPartyQueryResult
    isTopLevel = false
  } else {
    queryResult = {
      foundFirstPartyResource: false,
      foundThirdPartyResource: false,
      foundKnownThirdPartyAd: false
    }
    isTopLevel = true
  }

  if (elm.getAttribute) {
    if (elm.hasAttribute('id')) {
      const elmId = elm.getAttribute('id') as string
      if (elmId.startsWith('google_ads_iframe_') ||
          elmId.startsWith('div-gpt-ad') ||
          elmId.startsWith('adfox_')) {
        queryResult.foundKnownThirdPartyAd = true
        return false
      }
    }

    if (elm.hasAttribute('src')) {
      const elmSrc = elm.getAttribute('src') as string
      const elmSrcIsFirstParty = isFirstPartyUrl(elmSrc)
      if (elmSrcIsFirstParty === true) {
        queryResult.foundFirstPartyResource = true
        return true
      }
      queryResult.foundThirdPartyResource = true
    }

    if (elm.hasAttribute('style')) {
      const elmStyle = elm.getAttribute('style') as string
      if (elmStyle.includes('url(') ||
          elmStyle.includes('//')) {
        queryResult.foundThirdPartyResource = true
      }
    }

    if (elm.hasAttribute('srcdoc')) {
      const elmSrcDoc = elm.getAttribute('srcdoc') as string
      if (elmSrcDoc.trim() === "") {
        queryResult.foundThirdPartyResource = true
      }
    }
  }

  if (elm.firstChild) {
    isSubTreeFirstParty(elm.firstChild as Element, queryResult)
    if (queryResult.foundKnownThirdPartyAd === true) {
      return false
    }
    if (queryResult.foundFirstPartyResource === true) {
      return true
    }
  }

  if (elm.nextSibling) {
    isSubTreeFirstParty(elm.nextSibling as Element, queryResult)
    if (queryResult.foundKnownThirdPartyAd === true) {
      return false
    }
    if (queryResult.foundFirstPartyResource === true) {
      return true
    }
  }

  if (isTopLevel === false) {
    return (queryResult.foundThirdPartyResource === false)
  }

  const foundText = (elm as HTMLElement).innerText as string
  return (
    queryResult.foundThirdPartyResource === false &&
    foundText.trim().length > 0
  )
}

const hideSubtree = (elm: HTMLElement) => {
  if (elm.classList) {
    elm.classList.add('brave-cosmetic-filtering')
    elm.style.display = 'none'
  } else if (elm.parentNode) {
    (elm.parentNode as HTMLElement).removeChild(elm)
  }
}

// const customStyleRules = new Map()

const alreadyHiddenThirdPartySubtrees = new WeakSet()
const allSelectorsSet = new Set()
const firstRunQueue = new Set()
const secondRunQueue = new Set()
const finalRunQueue = new Set()
const allQueues = [firstRunQueue, secondRunQueue, finalRunQueue]
const numQueues = 3
const pumpIntervalMs = 50
const maxWorkSize = 25
let queueIsSleeping = false

const pumpQueues = () => {
  if (queueIsSleeping) {
    return
  }

  let didPumpAnything = false
  for (let queueIndex = 0; queueIndex < numQueues; queueIndex += 1) {
    const currentQueue = allQueues[queueIndex]
    const nextQueue = allQueues[queueIndex + 1]
    if (currentQueue.size === 0) {
      continue
    }

    const currentWorkLoad = Array.from(currentQueue.values()).slice(0, maxWorkSize)
    const comboSelector = currentWorkLoad.join(',')
    const matchingElms = Array.from(document.querySelectorAll(comboSelector))
    for (const aMatchingElm of matchingElms) {
      if (alreadyHiddenThirdPartySubtrees.has(aMatchingElm)) {
        continue
      }
      const elmSubtreeIsFirstParty = isSubTreeFirstParty(aMatchingElm)
      if (elmSubtreeIsFirstParty === false) {
        hideSubtree(aMatchingElm as HTMLElement)
        alreadyHiddenThirdPartySubtrees.add(aMatchingElm)
      }
    }

    for (const aUsedSelector of currentWorkLoad) {
      currentQueue.delete(aUsedSelector)
      if (nextQueue) {
        nextQueue.add(aUsedSelector)
      }
    }

    didPumpAnything = true
    break
  }

  if (didPumpAnything) {
    queueIsSleeping = true
    setTimeout(() => {
      queueIsSleeping = false
      pumpQueues()
    }, pumpIntervalMs)
  }
}

chrome.runtime.onMessage.addListener((msg, sender, sendResponse) => {
  const action = typeof msg === 'string' ? msg : msg.type
  switch (action) {
    case 'cosmeticFilterGenericExceptions': {
      const allNodes = Array.from(document.querySelectorAll('[id],[class]'))
      handleNewNodes(allNodes)
      applyCosmeticFilterMutationObserver()
      sendResponse(null)
      break
    }

    case 'cosmeticFilterConsiderNewRules': {
      const {hideRules} = msg
      for (const aHideRule of hideRules) {
        if (allSelectorsSet.has(aHideRule)) {
          continue
        }
        allSelectorsSet.add(aHideRule)
        firstRunQueue.add(aHideRule)
      }
      // for (const [aCustomStyleRule, aCustomStyle] of Object.entries(customStyleRules)) {
      //   if (allSelectorsSet.has(aCustomStyleRule)) {
      //     continue
      //   }
      //   allSelectorsSet.set(aCustomStyleRule, aCustomStyle)
      //   firstRunQueue.add(aCustomStyleRule)
      // }
      pumpQueues()
    }
  }
})
